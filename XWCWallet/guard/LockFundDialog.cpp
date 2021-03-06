#include "LockFundDialog.h"
#include "ui_LockFundDialog.h"

#include "wallet.h"
#include "commondialog.h"
#include "FeeChooseWidget.h"
#include "dialog/ErrorResultDialog.h"
#include "dialog/TransactionResultDialog.h"

LockFundDialog::LockFundDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LockFundDialog)
{
    ui->setupUi(this);

    connect( XWCWallet::getInstance(), SIGNAL(jsonDataUpdated(QString)), this, SLOT(jsonDataUpdated(QString)));

    setParent(XWCWallet::getInstance()->mainFrame->containerWidget);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::FramelessWindowHint);

    ui->widget->setObjectName("widget");
    ui->widget->setStyleSheet(BACKGROUNDWIDGET_STYLE);
    ui->containerWidget->setObjectName("containerwidget");
    ui->containerWidget->setStyleSheet(CONTAINERWIDGET_STYLE);

    ui->okBtn->setStyleSheet(OKBTN_STYLE);
    ui->cancelBtn->setStyleSheet(CANCELBTN_STYLE);
    ui->closeBtn->setStyleSheet(CLOSEBTN_STYLE);


    feeChoose = new FeeChooseWidget(0,XWCWallet::getInstance()->feeType);
    ui->stackedWidget->addWidget(feeChoose);
    feeChoose->resize(ui->stackedWidget->size());
    ui->stackedWidget->setCurrentIndex(0);

    init();

}

LockFundDialog::~LockFundDialog()
{
    delete ui;
}

void LockFundDialog::pop()
{
    move(0,0);
    exec();
}

void LockFundDialog::init()
{
    ui->accountComboBox->addItems(XWCWallet::getInstance()->accountInfoMap.keys());

    QStringList keys = XWCWallet::getInstance()->assetInfoMap.keys();
    foreach (QString key, keys)
    {
        ui->assetComboBox->addItem( revertERCSymbol( XWCWallet::getInstance()->assetInfoMap.value(key).symbol), key);
    }

    ui->okBtn->setEnabled(false);
}

void LockFundDialog::setAccount(QString accountName)
{
    ui->accountComboBox->setCurrentText(accountName);
}

void LockFundDialog::jsonDataUpdated(QString id)
{
    if( id == "LockFundDialog+transfer_to_contract_testing")
    {
        QString result = XWCWallet::getInstance()->jsonDataValue(id);

        if(result.startsWith("\"result\":"))
        {
            XWCWallet::TotalContractFee totalFee = XWCWallet::getInstance()->parseTotalContractFee(result);
            stepCount = totalFee.step;
            unsigned long long totalAmount = totalFee.baseAmount + ceil(totalFee.step * XWCWallet::getInstance()->contractFee / 100.0);

            feeChoose->updateFeeNumberSlots(getBigNumberString(totalAmount, ASSET_PRECISION).toDouble());
            feeChoose->updateAccountNameSlots(ui->accountComboBox->currentText(),true);

            checkOkBtnEnabled();
        }

        return;
    }

    if( id == "LockFundDialog+unlock" )
    {
        QString result = XWCWallet::getInstance()->jsonDataValue(id);
        qDebug() << id << result;

        if( result == "\"result\":null")
        {
            QString contractAddress = XWCWallet::getInstance()->getExchangeContractAddress(ui->accountComboBox->currentText());

            feeChoose->updatePoundageID();
            XWCWallet::getInstance()->postRPC( "LockFundDialog+transfer_to_contract", toJsonFormat( "transfer_to_contract",
                                                                                   QJsonArray() << ui->accountComboBox->currentText() << LOCKFUND_CONTRACT_ADDRESS
                                                                                   << ui->amountLineEdit->text() << getRealAssetSymbol( ui->assetComboBox->currentText())
                                                                                   << ""
                                                                                   << XWCWallet::getInstance()->currentContractFee() << stepCount
                                                                                   << true
                                                                                   ));

        }
        else if(result.startsWith("\"error\":"))
        {
            ui->okBtn->setEnabled(true);
            ui->tipLabel->setText("<body><font style=\"font-size:12px\" color=#ff224c>" + tr("Wrong password!") + "</font></body>" );
        }

        return;
    }

    if( id == "LockFundDialog+transfer_to_contract")
    {
        QString result = XWCWallet::getInstance()->jsonDataValue(id);
        qDebug() << id << result;

        if(result.startsWith("\"result\":"))
        {
            close();

            TransactionResultDialog transactionResultDialog;
            transactionResultDialog.setInfoText(tr("Transaction of lock-position has been sent out!"));
            transactionResultDialog.setDetailText(result);
            transactionResultDialog.pop();
        }
        else if(result.startsWith("\"error\":"))
        {
            ErrorResultDialog errorResultDialog;
            errorResultDialog.setInfoText(tr("Fail to lock position to the contract!"));
            errorResultDialog.setDetailText(result);
            errorResultDialog.pop();
        }

        return;
    }
}

void LockFundDialog::on_okBtn_clicked()
{
    if(!XWCWallet::getInstance()->ValidateOnChainOperation())     return;

    if(ui->amountLineEdit->text().toDouble() <= 0)  return;
    if(ui->pwdLineEdit->text().isEmpty())           return;

    ui->okBtn->setEnabled(false);
    XWCWallet::getInstance()->postRPC( "LockFundDialog+unlock", toJsonFormat( "unlock", QJsonArray() << ui->pwdLineEdit->text()
                                               ));
}

void LockFundDialog::on_cancelBtn_clicked()
{
    close();
}

void LockFundDialog::on_closeBtn_clicked()
{
    close();
}

void LockFundDialog::on_accountComboBox_currentIndexChanged(const QString &arg1)
{
    resetAmountLineEdit();
}

void LockFundDialog::on_assetComboBox_currentIndexChanged(const QString &arg1)
{
    resetAmountLineEdit();
}


void LockFundDialog::resetAmountLineEdit()
{
    AssetAmount assetAmount = XWCWallet::getInstance()->accountInfoMap.value(ui->accountComboBox->currentText()).assetAmountMap.value(ui->assetComboBox->currentData().toString());
    QString amountStr = getBigNumberString(assetAmount.amount, XWCWallet::getInstance()->assetInfoMap.value(ui->assetComboBox->currentData().toString()).precision);

    ui->amountLineEdit->setPlaceholderText(tr("Max: %1 %2").arg(amountStr).arg( revertERCSymbol( ui->assetComboBox->currentText())) );

    AssetInfo assetInfo = XWCWallet::getInstance()->assetInfoMap.value(XWCWallet::getInstance()->getAssetId( getRealAssetSymbol( ui->assetComboBox->currentText())));
    QRegExp rx1(QString("^([0]|[1-9][0-9]{0,10})(?:\\.\\d{0,%1})?$|(^\\t?$)").arg(assetInfo.precision));
    QRegExpValidator *pReg1 = new QRegExpValidator(rx1, this);
    ui->amountLineEdit->setValidator(pReg1);
    ui->amountLineEdit->clear();

    stepCount = 0;
    checkOkBtnEnabled();
}

void LockFundDialog::checkOkBtnEnabled()
{
    if(stepCount > 0 && !ui->pwdLineEdit->text().isEmpty())
    {
        ui->okBtn->setEnabled(true);
    }
    else
    {
        ui->okBtn->setEnabled(false);
    }
}


void LockFundDialog::on_amountLineEdit_textEdited(const QString &arg1)
{
    stepCount = 0;
    checkOkBtnEnabled();
    if(ui->amountLineEdit->text().isEmpty())    return;

    QString assetId = XWCWallet::getInstance()->getAssetId( getRealAssetSymbol( ui->assetComboBox->currentText()));
    AssetAmount assetAmount = XWCWallet::getInstance()->accountInfoMap.value(ui->accountComboBox->currentText()).assetAmountMap.value(assetId);
    QString amountStr = getBigNumberString(assetAmount.amount, XWCWallet::getInstance()->assetInfoMap.value(assetId).precision);

    if(ui->amountLineEdit->text().toDouble() > amountStr.toDouble())
    {
        ui->amountLineEdit->setText(amountStr);
    }

    XWCWallet::getInstance()->postRPC( "LockFundDialog+transfer_to_contract_testing",
                                     toJsonFormat( "transfer_to_contract_testing",
                                                   QJsonArray() << ui->accountComboBox->currentText()
                                                   << LOCKFUND_CONTRACT_ADDRESS
                                                   << ui->amountLineEdit->text()
                                                   << getRealAssetSymbol( ui->assetComboBox->currentText())
                                                   << "" ));
}

void LockFundDialog::on_pwdLineEdit_textChanged(const QString &arg1)
{
    ui->tipLabel->clear();
    checkOkBtnEnabled();
}
