/********************************************************************************
** Form generated from reading ui file 'select.ui'
**
** Created: Mon May 5 21:13:44 2008
**      by: Qt User Interface Compiler version 4.4.0-rc1
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_SELECT_H
#define UI_SELECT_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_SelectDialog
{
public:
    QVBoxLayout *vboxLayout;
    QGridLayout *gridLayout;
    QLabel *message;
    QLabel *icon;
    QHBoxLayout *hboxLayout;
    QComboBox *select;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
    QSpacerItem *spacerItem2;
    QHBoxLayout *hboxLayout1;
    QSpacerItem *spacerItem3;
    QPushButton *ok;
    QPushButton *cancel;
    QSpacerItem *spacerItem4;

    void setupUi(QDialog *SelectDialog)
    {
    if (SelectDialog->objectName().isEmpty())
        SelectDialog->setObjectName(QString::fromUtf8("SelectDialog"));
    SelectDialog->resize(222, 141);
    SelectDialog->setSizeGripEnabled(false);
    vboxLayout = new QVBoxLayout(SelectDialog);
    vboxLayout->setSpacing(6);
    vboxLayout->setMargin(11);
    vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    message = new QLabel(SelectDialog);
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

    icon = new QLabel(SelectDialog);
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

    hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
    select = new QComboBox(SelectDialog);
    select->setObjectName(QString::fromUtf8("select"));
    QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(select->sizePolicy().hasHeightForWidth());
    select->setSizePolicy(sizePolicy2);

    hboxLayout->addWidget(select);

    spacerItem = new QSpacerItem(10, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    hboxLayout->addItem(spacerItem);


    gridLayout->addLayout(hboxLayout, 1, 1, 1, 1);

    spacerItem1 = new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum);

    gridLayout->addItem(spacerItem1, 1, 0, 1, 1);


    vboxLayout->addLayout(gridLayout);

    spacerItem2 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    vboxLayout->addItem(spacerItem2);

    hboxLayout1 = new QHBoxLayout();
    hboxLayout1->setSpacing(6);
    hboxLayout1->setObjectName(QString::fromUtf8("hboxLayout1"));
    spacerItem3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout1->addItem(spacerItem3);

    ok = new QPushButton(SelectDialog);
    ok->setObjectName(QString::fromUtf8("ok"));
    ok->setAutoDefault(true);
    ok->setDefault(true);

    hboxLayout1->addWidget(ok);

    cancel = new QPushButton(SelectDialog);
    cancel->setObjectName(QString::fromUtf8("cancel"));
    cancel->setAutoDefault(true);

    hboxLayout1->addWidget(cancel);

    spacerItem4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hboxLayout1->addItem(spacerItem4);


    vboxLayout->addLayout(hboxLayout1);

    QWidget::setTabOrder(select, ok);
    QWidget::setTabOrder(ok, cancel);

    retranslateUi(SelectDialog);
    QObject::connect(ok, SIGNAL(clicked()), SelectDialog, SLOT(accept()));
    QObject::connect(cancel, SIGNAL(clicked()), SelectDialog, SLOT(reject()));

    QMetaObject::connectSlotsByName(SelectDialog);
    } // setupUi

    void retranslateUi(QDialog *SelectDialog)
    {
    SelectDialog->setWindowTitle(QApplication::translate("SelectDialog", "Select", 0, QApplication::UnicodeUTF8));
    message->setText(QApplication::translate("SelectDialog", "select text", 0, QApplication::UnicodeUTF8));
    ok->setText(QApplication::translate("SelectDialog", "&OK", 0, QApplication::UnicodeUTF8));
    cancel->setText(QApplication::translate("SelectDialog", "&Cancel", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(SelectDialog);
    } // retranslateUi

};

namespace Ui {
    class SelectDialog: public Ui_SelectDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SELECT_H
