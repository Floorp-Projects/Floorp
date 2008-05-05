/********************************************************************************
** Form generated from reading ui file 'alert.ui'
**
** Created: Mon May 5 19:26:41 2008
**      by: Qt User Interface Compiler version 4.4.0-rc1
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_ALERT_H
#define UI_ALERT_H

#include <Qt3Support/Q3MimeSourceFactory>
#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_AlertDialog
{
public:
    QVBoxLayout *vboxLayout;
    QGridLayout *gridLayout;
    QSpacerItem *spacerItem;
    QLabel *message;
    QSpacerItem *spacerItem1;
    QCheckBox *check;
    QSpacerItem *spacerItem2;
    QLabel *icon;
    QSpacerItem *spacerItem3;
    QHBoxLayout *hboxLayout;
    QSpacerItem *spacerItem4;
    QPushButton *ok;
    QSpacerItem *spacerItem5;

    void setupUi(QDialog *AlertDialog)
    {
    if (AlertDialog->objectName().isEmpty())
        AlertDialog->setObjectName(QString::fromUtf8("AlertDialog"));
    AlertDialog->resize(187, 122);
    AlertDialog->setSizeGripEnabled(false);
    vboxLayout = new QVBoxLayout(AlertDialog);
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(11);
    vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    spacerItem = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem, 1, 1, 1, 1);

    message = new QLabel(AlertDialog);
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

    spacerItem1 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem1, 2, 0, 1, 1);

    check = new QCheckBox(AlertDialog);
    check->setObjectName(QString::fromUtf8("check"));
    QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(check->sizePolicy().hasHeightForWidth());
    check->setSizePolicy(sizePolicy1);

    gridLayout->addWidget(check, 2, 1, 1, 1);

    spacerItem2 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem2, 1, 0, 1, 1);

    icon = new QLabel(AlertDialog);
    icon->setObjectName(QString::fromUtf8("icon"));
    QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Minimum);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(icon->sizePolicy().hasHeightForWidth());
    icon->setSizePolicy(sizePolicy2);
    icon->setScaledContents(false);
    icon->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
    icon->setWordWrap(false);

    gridLayout->addWidget(icon, 0, 0, 1, 1);


    vboxLayout->addLayout(gridLayout);

    spacerItem3 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    vboxLayout->addItem(spacerItem3);

    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
    spacerItem4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem4);

    ok = new QPushButton(AlertDialog);
    ok->setObjectName(QString::fromUtf8("ok"));
    ok->setAutoDefault(true);
    ok->setDefault(true);

    hboxLayout->addWidget(ok);

    spacerItem5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem5);


    vboxLayout->addLayout(hboxLayout);

    QWidget::setTabOrder(ok, check);

    retranslateUi(AlertDialog);
    QObject::connect(ok, SIGNAL(clicked()), AlertDialog, SLOT(accept()));

    QMetaObject::connectSlotsByName(AlertDialog);
    } // setupUi

    void retranslateUi(QDialog *AlertDialog)
    {
    AlertDialog->setWindowTitle(QApplication::translate("AlertDialog", "Alert", 0, QApplication::UnicodeUTF8));
    message->setText(QApplication::translate("AlertDialog", "alert text", 0, QApplication::UnicodeUTF8));
    check->setText(QApplication::translate("AlertDialog", "confirm", 0, QApplication::UnicodeUTF8));
    ok->setText(QApplication::translate("AlertDialog", "&OK", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(AlertDialog);
    } // retranslateUi

};

namespace Ui {
    class AlertDialog: public Ui_AlertDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ALERT_H
