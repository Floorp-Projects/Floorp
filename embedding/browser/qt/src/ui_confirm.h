/********************************************************************************
** Form generated from reading ui file 'confirm.ui'
**
** Created: Mon May 5 20:48:56 2008
**      by: Qt User Interface Compiler version 4.4.0-rc1
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_CONFIRM_H
#define UI_CONFIRM_H

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

class Ui_ConfirmDialog
{
public:
    QVBoxLayout *vboxLayout;
    QGridLayout *gridLayout;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QCheckBox *check;
    QLabel *message;
    QLabel *icon;
    QSpacerItem *spacerItem2;
    QSpacerItem *spacerItem3;
    QHBoxLayout *hboxLayout;
    QSpacerItem *spacerItem4;
    QPushButton *but1;
    QPushButton *but2;
    QPushButton *but3;
    QSpacerItem *spacerItem5;

    void setupUi(QDialog *ConfirmDialog)
    {
    if (ConfirmDialog->objectName().isEmpty())
        ConfirmDialog->setObjectName(QString::fromUtf8("ConfirmDialog"));
    ConfirmDialog->resize(296, 152);
    QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(ConfirmDialog->sizePolicy().hasHeightForWidth());
    ConfirmDialog->setSizePolicy(sizePolicy);
    ConfirmDialog->setSizeGripEnabled(false);
    vboxLayout = new QVBoxLayout(ConfirmDialog);
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(11);
    vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    spacerItem = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem, 1, 0, 1, 1);

    spacerItem1 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem1, 2, 0, 1, 1);

    check = new QCheckBox(ConfirmDialog);
    check->setObjectName(QString::fromUtf8("check"));
    QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(check->sizePolicy().hasHeightForWidth());
    check->setSizePolicy(sizePolicy1);

    gridLayout->addWidget(check, 2, 1, 1, 1);

    message = new QLabel(ConfirmDialog);
    message->setObjectName(QString::fromUtf8("message"));
    message->setEnabled(true);
    QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(message->sizePolicy().hasHeightForWidth());
    message->setSizePolicy(sizePolicy2);
    message->setAlignment(Qt::AlignVCenter);
    message->setWordWrap(true);

    gridLayout->addWidget(message, 0, 1, 1, 1);

    icon = new QLabel(ConfirmDialog);
    icon->setObjectName(QString::fromUtf8("icon"));
    sizePolicy.setHeightForWidth(icon->sizePolicy().hasHeightForWidth());
    icon->setSizePolicy(sizePolicy);
    icon->setScaledContents(false);
    icon->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
    icon->setWordWrap(false);

    gridLayout->addWidget(icon, 0, 0, 1, 1);

    spacerItem2 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem2, 1, 1, 1, 1);


    vboxLayout->addLayout(gridLayout);

    spacerItem3 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    vboxLayout->addItem(spacerItem3);

    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
    spacerItem4 = new QSpacerItem(5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem4);

    but1 = new QPushButton(ConfirmDialog);
    but1->setObjectName(QString::fromUtf8("but1"));
    but1->setAutoDefault(true);
    but1->setDefault(true);

    hboxLayout->addWidget(but1);

    but2 = new QPushButton(ConfirmDialog);
    but2->setObjectName(QString::fromUtf8("but2"));
    but2->setAutoDefault(true);

    hboxLayout->addWidget(but2);

    but3 = new QPushButton(ConfirmDialog);
    but3->setObjectName(QString::fromUtf8("but3"));
    but3->setAutoDefault(true);

    hboxLayout->addWidget(but3);

    spacerItem5 = new QSpacerItem(5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem5);


    vboxLayout->addLayout(hboxLayout);


    retranslateUi(ConfirmDialog);
    QObject::connect(but1, SIGNAL(clicked()), ConfirmDialog, SLOT(done1()));
    QObject::connect(but2, SIGNAL(clicked()), ConfirmDialog, SLOT(done2()));
    QObject::connect(but3, SIGNAL(clicked()), ConfirmDialog, SLOT(done3()));

    QMetaObject::connectSlotsByName(ConfirmDialog);
    } // setupUi

    void retranslateUi(QDialog *ConfirmDialog)
    {
    ConfirmDialog->setWindowTitle(QApplication::translate("ConfirmDialog", "Confirm", 0, QApplication::UnicodeUTF8));
    check->setText(QApplication::translate("ConfirmDialog", "confirm", 0, QApplication::UnicodeUTF8));
    message->setText(QApplication::translate("ConfirmDialog", "confirm text", 0, QApplication::UnicodeUTF8));
    but1->setText(QApplication::translate("ConfirmDialog", "1", 0, QApplication::UnicodeUTF8));
    but2->setText(QApplication::translate("ConfirmDialog", "2", 0, QApplication::UnicodeUTF8));
    but3->setText(QApplication::translate("ConfirmDialog", "3", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(ConfirmDialog);
    } // retranslateUi

};

namespace Ui {
    class ConfirmDialog: public Ui_ConfirmDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIRM_H
