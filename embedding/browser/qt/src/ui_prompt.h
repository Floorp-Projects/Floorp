/********************************************************************************
** Form generated from reading ui file 'prompt.ui'
**
** Created: Mon May 5 21:01:48 2008
**      by: Qt User Interface Compiler version 4.4.0-rc1
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_PROMPT_H
#define UI_PROMPT_H

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

class Ui_PromptDialog
{
public:
    QVBoxLayout *vboxLayout;
    QGridLayout *gridLayout;
    QLabel *message;
    QLabel *icon;
    QCheckBox *check;
    QHBoxLayout *hboxLayout;
    QLineEdit *input;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QSpacerItem *spacerItem2;
    QSpacerItem *spacerItem3;
    QSpacerItem *spacerItem4;
    QSpacerItem *spacerItem5;
    QHBoxLayout *hboxLayout1;
    QSpacerItem *spacerItem6;
    QPushButton *ok;
    QPushButton *cancel;
    QSpacerItem *spacerItem7;

    void setupUi(QDialog *PromptDialog)
    {
    if (PromptDialog->objectName().isEmpty())
        PromptDialog->setObjectName(QString::fromUtf8("PromptDialog"));
    PromptDialog->resize(222, 177);
    PromptDialog->setSizeGripEnabled(false);
    vboxLayout = new QVBoxLayout(PromptDialog);
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(11);
    vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    message = new QLabel(PromptDialog);
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

    icon = new QLabel(PromptDialog);
    icon->setObjectName(QString::fromUtf8("icon"));
    QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Minimum);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(icon->sizePolicy().hasHeightForWidth());
    icon->setSizePolicy(sizePolicy1);
    icon->setScaledContents(false);
    icon->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
    icon->setWordWrap(false);

    gridLayout->addWidget(icon, 0, 0, 1, 1);

    check = new QCheckBox(PromptDialog);
    check->setObjectName(QString::fromUtf8("check"));
    QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(check->sizePolicy().hasHeightForWidth());
    check->setSizePolicy(sizePolicy2);

    gridLayout->addWidget(check, 3, 1, 1, 1);

    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
    input = new QLineEdit(PromptDialog);
    input->setObjectName(QString::fromUtf8("input"));
    sizePolicy2.setHeightForWidth(input->sizePolicy().hasHeightForWidth());
    input->setSizePolicy(sizePolicy2);

    hboxLayout->addWidget(input);

    spacerItem = new QSpacerItem(10, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem);


    gridLayout->addLayout(hboxLayout, 1, 1, 1, 1);

    spacerItem1 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem1, 3, 0, 1, 1);

    spacerItem2 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem2, 2, 1, 1, 1);

    spacerItem3 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem3, 2, 0, 1, 1);

    spacerItem4 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem4, 1, 0, 1, 1);


    vboxLayout->addLayout(gridLayout);

    spacerItem5 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    vboxLayout->addItem(spacerItem5);

    hboxLayout1 = new QHBoxLayout();
    hboxLayout1->setSpacing(6);
    hboxLayout1->setObjectName(QString::fromUtf8("hboxLayout1"));
    spacerItem6 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout1->addItem(spacerItem6);

    ok = new QPushButton(PromptDialog);
    ok->setObjectName(QString::fromUtf8("ok"));
    ok->setAutoDefault(true);
    ok->setDefault(true);

    hboxLayout1->addWidget(ok);

    cancel = new QPushButton(PromptDialog);
    cancel->setObjectName(QString::fromUtf8("cancel"));
    cancel->setAutoDefault(true);

    hboxLayout1->addWidget(cancel);

    spacerItem7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout1->addItem(spacerItem7);


    vboxLayout->addLayout(hboxLayout1);

    QWidget::setTabOrder(input, check);
    QWidget::setTabOrder(check, ok);
    QWidget::setTabOrder(ok, cancel);

    retranslateUi(PromptDialog);
    QObject::connect(ok, SIGNAL(clicked()), PromptDialog, SLOT(accept()));
    QObject::connect(cancel, SIGNAL(clicked()), PromptDialog, SLOT(reject()));

    QMetaObject::connectSlotsByName(PromptDialog);
    } // setupUi

    void retranslateUi(QDialog *PromptDialog)
    {
    PromptDialog->setWindowTitle(QApplication::translate("PromptDialog", "Prompt", 0, QApplication::UnicodeUTF8));
    message->setText(QApplication::translate("PromptDialog", "prompt text", 0, QApplication::UnicodeUTF8));
    check->setText(QApplication::translate("PromptDialog", "confirm", 0, QApplication::UnicodeUTF8));
    ok->setText(QApplication::translate("PromptDialog", "&OK", 0, QApplication::UnicodeUTF8));
    cancel->setText(QApplication::translate("PromptDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(PromptDialog);
    } // retranslateUi

};

namespace Ui {
    class PromptDialog: public Ui_PromptDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROMPT_H
