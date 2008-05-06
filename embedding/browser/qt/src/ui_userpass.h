/********************************************************************************
** Form generated from reading ui file 'userpass.ui'
**
** Created: Mon May 5 21:09:17 2008
**      by: Qt User Interface Compiler version 4.4.0-rc1
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_USERPASS_H
#define UI_USERPASS_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_UserpassDialog
{
public:
    QVBoxLayout *vboxLayout;
    QGridLayout *gridLayout;
    QLabel *message;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QLabel *icon;
    QGridLayout *gridLayout1;
    QLineEdit *username;
    QSpacerItem *spacerItem2;
    QLineEdit *password;
    QLabel *lb_password;
    QSpacerItem *spacerItem3;
    QLabel *lb_username;
    QSpacerItem *spacerItem4;
    QCheckBox *check;
    QSpacerItem *spacerItem5;
    QSpacerItem *spacerItem6;
    QHBoxLayout *hboxLayout;
    QSpacerItem *spacerItem7;
    QPushButton *ok;
    QPushButton *cancel;
    QSpacerItem *spacerItem8;

    void setupUi(QDialog *UserpassDialog)
    {
    if (UserpassDialog->objectName().isEmpty())
        UserpassDialog->setObjectName(QString::fromUtf8("UserpassDialog"));
    UserpassDialog->resize(264, 204);
    vboxLayout = new QVBoxLayout(UserpassDialog);
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(11);
    vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    message = new QLabel(UserpassDialog);
    message->setObjectName(QString::fromUtf8("message"));
    message->setEnabled(true);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(message->sizePolicy().hasHeightForWidth());
    message->setSizePolicy(sizePolicy);
    message->setAlignment(Qt::AlignVCenter);
    message->setWordWrap(true);

    gridLayout->addWidget(message, 0, 1, 1, 1);

    spacerItem = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem, 3, 0, 1, 1);

    spacerItem1 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem1, 2, 1, 1, 1);

    icon = new QLabel(UserpassDialog);
    icon->setObjectName(QString::fromUtf8("icon"));
    QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Minimum);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(icon->sizePolicy().hasHeightForWidth());
    icon->setSizePolicy(sizePolicy1);
    icon->setScaledContents(false);
    icon->setWordWrap(false);

    gridLayout->addWidget(icon, 0, 0, 1, 1);

    gridLayout1 = new QGridLayout();
    gridLayout1->setSpacing(6);
    gridLayout1->setObjectName(QString::fromUtf8("gridLayout1"));
    username = new QLineEdit(UserpassDialog);
    username->setObjectName(QString::fromUtf8("username"));

    gridLayout1->addWidget(username, 0, 1, 1, 1);

    spacerItem2 = new QSpacerItem(10, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout1->addItem(spacerItem2, 1, 2, 1, 1);

    password = new QLineEdit(UserpassDialog);
    password->setObjectName(QString::fromUtf8("password"));
    password->setEchoMode(QLineEdit::Password);

    gridLayout1->addWidget(password, 1, 1, 1, 1);

    lb_password = new QLabel(UserpassDialog);
    lb_password->setObjectName(QString::fromUtf8("lb_password"));
    lb_password->setWordWrap(false);

    gridLayout1->addWidget(lb_password, 1, 0, 1, 1);

    spacerItem3 = new QSpacerItem(10, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout1->addItem(spacerItem3, 0, 2, 1, 1);

    lb_username = new QLabel(UserpassDialog);
    lb_username->setObjectName(QString::fromUtf8("lb_username"));
    lb_username->setWordWrap(false);

    gridLayout1->addWidget(lb_username, 0, 0, 1, 1);


    gridLayout->addLayout(gridLayout1, 1, 1, 1, 1);

    spacerItem4 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem4, 1, 0, 1, 1);

    check = new QCheckBox(UserpassDialog);
    check->setObjectName(QString::fromUtf8("check"));
    QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(check->sizePolicy().hasHeightForWidth());
    check->setSizePolicy(sizePolicy2);

    gridLayout->addWidget(check, 3, 1, 1, 1);

    spacerItem5 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem5, 2, 0, 1, 1);


    vboxLayout->addLayout(gridLayout);

    spacerItem6 = new QSpacerItem(20, 16, QSizePolicy::Minimum, QSizePolicy::Fixed);

    vboxLayout->addItem(spacerItem6);

    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
    spacerItem7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem7);

    ok = new QPushButton(UserpassDialog);
    ok->setObjectName(QString::fromUtf8("ok"));
    ok->setAutoDefault(true);
    ok->setDefault(true);

    hboxLayout->addWidget(ok);

    cancel = new QPushButton(UserpassDialog);
    cancel->setObjectName(QString::fromUtf8("cancel"));

    hboxLayout->addWidget(cancel);

    spacerItem8 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem8);


    vboxLayout->addLayout(hboxLayout);

    QWidget::setTabOrder(username, password);
    QWidget::setTabOrder(password, check);
    QWidget::setTabOrder(check, ok);
    QWidget::setTabOrder(ok, cancel);

    retranslateUi(UserpassDialog);
    QObject::connect(ok, SIGNAL(clicked()), UserpassDialog, SLOT(accept()));
    QObject::connect(cancel, SIGNAL(clicked()), UserpassDialog, SLOT(reject()));
    QObject::connect(password, SIGNAL(returnPressed()), UserpassDialog, SLOT(accept()));

    QMetaObject::connectSlotsByName(UserpassDialog);
    } // setupUi

    void retranslateUi(QDialog *UserpassDialog)
    {
    UserpassDialog->setWindowTitle(QApplication::translate("UserpassDialog", "Prompt", 0, QApplication::UnicodeUTF8));
    message->setText(QApplication::translate("UserpassDialog", "prompt text", 0, QApplication::UnicodeUTF8));
    lb_password->setText(QApplication::translate("UserpassDialog", "Password:", 0, QApplication::UnicodeUTF8));
    lb_username->setText(QApplication::translate("UserpassDialog", "Username:", 0, QApplication::UnicodeUTF8));
    check->setText(QApplication::translate("UserpassDialog", "confirm", 0, QApplication::UnicodeUTF8));
    ok->setText(QApplication::translate("UserpassDialog", "&OK", 0, QApplication::UnicodeUTF8));
    cancel->setText(QApplication::translate("UserpassDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(UserpassDialog);
    } // retranslateUi

};

namespace Ui {
    class UserpassDialog: public Ui_UserpassDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_USERPASS_H
