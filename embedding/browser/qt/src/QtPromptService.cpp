/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Michal Ceresna
 * for Lixto GmbH.
 *
 * Portions created by Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michal Ceresna <ceresna@amos.sturak.sk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "QtPromptService.h"
#include <nsString.h>
#include <nsIWindowWatcher.h>
#include <nsIWebBrowserChrome.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsCOMPtr.h>
#include <nsIServiceManager.h>
#include <nsReadableUtils.h>

#include <qmessagebox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qapplication.h>
#include <qstyle.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include "ui_alert.h"
#include "ui_confirm.h"
#include "ui_prompt.h"
#include "ui_select.h"
#include "ui_userpass.h"

#if (QT_VERSION < 0x030200)
//constant not defined in older qt version
#define SP_MessageBoxQuestion SP_MessageBoxInformation
#endif

QtPromptService::QtPromptService()
{
}

QtPromptService::~QtPromptService()
{
}

NS_IMPL_ISUPPORTS1(QtPromptService, nsIPromptService)

/**
 * Puts up an alert dialog with an OK button.
 */
NS_IMETHODIMP
QtPromptService::Alert(nsIDOMWindow* aParent,
                       const PRUnichar* aDialogTitle,
                       const PRUnichar* aDialogText)
{
    return
        AlertCheck(aParent,
                   aDialogTitle, aDialogText,
                   NULL, NULL);
}

/**
 * Puts up an alert dialog with an OK button and
 * a message with a checkbox.
 */
NS_IMETHODIMP
QtPromptService::AlertCheck(nsIDOMWindow* aParent,
                            const PRUnichar* aDialogTitle,
                            const PRUnichar* aDialogText,
                            const PRUnichar* aCheckMsg,
                            PRBool* aCheckValue)
{
    AlertDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxWarning));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));
    if (aCheckMsg) {
        d.check->setText(QString::fromUcs2(aCheckMsg));
        d.check->setChecked(*aCheckValue);
    }
    else {
        d.check->hide();
    }
    d.adjustSize();
    d.exec();

    if (aCheckMsg) {
        *aCheckValue = d.check->isChecked();
    }

    return NS_OK;
}

/**
 * Puts up a dialog with OK and Cancel buttons.
 * @return true for OK, false for Cancel
 */
NS_IMETHODIMP
QtPromptService::Confirm(nsIDOMWindow* aParent,
                         const PRUnichar* aDialogTitle,
                         const PRUnichar* aDialogText,
                         PRBool* aConfirm)
{
    return
        ConfirmCheck(aParent,
                     aDialogTitle, aDialogText,
                     NULL, NULL,
                     aConfirm);
}

/**
 * Puts up a dialog with OK and Cancel buttons, and
 * a message with a single checkbox.
 * @return true for OK, false for Cancel
 */
NS_IMETHODIMP
QtPromptService::ConfirmCheck(nsIDOMWindow* aParent,
                              const PRUnichar* aDialogTitle,
                              const PRUnichar* aDialogText,
                              const PRUnichar* aCheckMsg,
                              PRBool* aCheckValue,
                              PRBool* aConfirm)
{
    PRInt32 ret;
    ConfirmEx(aParent,
              aDialogTitle, aDialogText,
              STD_OK_CANCEL_BUTTONS,
              NULL, NULL, NULL,
              aCheckMsg,
              aCheckValue,
              &ret);
    *aConfirm = (ret==0);

    return NS_OK;
}

/**
 * Puts up a dialog with up to 3 buttons and an optional checkbox.
 *
 * @param dialogTitle
 * @param text
 * @param buttonFlags       Title flags for each button.
 * @param button0Title      Used when button 0 uses TITLE_IS_STRING
 * @param button1Title      Used when button 1 uses TITLE_IS_STRING
 * @param button2Title      Used when button 2 uses TITLE_IS_STRING
 * @param checkMsg          null if no checkbox
 * @param checkValue
 * @return buttonPressed
 *
 * Buttons are numbered 0 - 2. The implementation can decide whether
 * the sequence goes from right to left or left to right.
 * Button 0 will be the default button.
 *
 * A button may use a predefined title, specified by one of the
 * constants below. Each title constant can be multiplied by a
 * position constant to assign the title to a particular button.
 * If BUTTON_TITLE_IS_STRING is used for a button, the string
 * parameter for that button will be used. If the value for a button
 * position is zero, the button will not be shown
 *
 */
NS_IMETHODIMP
QtPromptService::ConfirmEx(nsIDOMWindow* aParent,
                           const PRUnichar* aDialogTitle,
                           const PRUnichar* aDialogText,
                           PRUint32 aButtonFlags,
                           const PRUnichar* aButton0Title,
                           const PRUnichar* aButton1Title,
                           const PRUnichar* aButton2Title,
                           const PRUnichar* aCheckMsg,
                           PRBool* aCheckValue,
                           PRInt32* aRetVal)
{
    ConfirmDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxQuestion));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));

    QString l = GetButtonLabel(aButtonFlags, BUTTON_POS_0, aButton0Title);
    if (!l.isNull()) d.but1->setText(l); else d.but1->hide();
    l = GetButtonLabel(aButtonFlags, BUTTON_POS_1, aButton1Title);
    if (!l.isNull()) d.but2->setText(l); else d.but2->hide();
    l = GetButtonLabel(aButtonFlags, BUTTON_POS_2, aButton2Title);
    if (!l.isNull()) d.but3->setText(l); else d.but3->hide();

    if (aCheckMsg) {
        d.check->setText(QString::fromUcs2(aCheckMsg));
        d.check->setChecked(*aCheckValue);
    }
    else {
        d.check->hide();
    }
    d.adjustSize();
    int ret = d.exec();

    *aRetVal = ret;

    return NS_OK;
}

/**
 * Puts up a dialog with an edit field and an optional checkbox.
 *
 * @param dialogTitle
 * @param text
 * @param value         in: Pre-fills the dialog field if non-null
 *                      out: If result is true, a newly allocated
 *                      string. If result is false, in string is not
 *                      touched.
 * @param checkMsg      if null, check box will not be shown
 * @param checkValue
 * @return true for OK, false for Cancel
 */
NS_IMETHODIMP
QtPromptService::Prompt(nsIDOMWindow* aParent,
                        const PRUnichar* aDialogTitle,
                        const PRUnichar* aDialogText,
                        PRUnichar** aValue,
                        const PRUnichar* aCheckMsg,
                        PRBool* aCheckValue,
                        PRBool* aConfirm)
{
    PromptDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxQuestion));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));
    if (aValue && *aValue) {
        d.input->setText(QString::fromUcs2(*aValue));
    }
    if (aCheckMsg) {
        d.check->setText(QString::fromUcs2(aCheckMsg));
        d.check->setChecked(*aCheckValue);
    }
    else {
        d.check->hide();
    }
    d.adjustSize();
    int ret = d.exec();

    if (aCheckMsg) {
        *aCheckValue = d.check->isChecked();
    }
    *aConfirm = (ret & QMessageBox::Ok);
    if (*aConfirm) {
        if (*aValue) nsMemory::Free(*aValue);
        *aValue =
            ToNewUnicode(NS_ConvertUTF8toUTF16(d.input->text().utf8()));
    }

    return NS_OK;
}

/**
 * Puts up a dialog with an edit field, a password field, and an optional checkbox.
 *
 * @param dialogTitle
 * @param text
 * @param username      in: Pre-fills the dialog field if non-null
 *                      out: If result is true, a newly allocated
 *                      string. If result is false, in string is not
 *                      touched.
 * @param password      in: Pre-fills the dialog field if non-null
 *                      out: If result is true, a newly allocated
 *                      string. If result is false, in string is not
 *                      touched.
 * @param checkMsg      if null, check box will not be shown
 * @param checkValue
 * @return true for OK, false for Cancel
 */
NS_IMETHODIMP
QtPromptService::PromptUsernameAndPassword(nsIDOMWindow* aParent,
                                           const PRUnichar* aDialogTitle,
                                           const PRUnichar* aDialogText,
                                           PRUnichar** aUsername,
                                           PRUnichar** aPassword,
                                           const PRUnichar* aCheckMsg,
                                           PRBool* aCheckValue,
                                           PRBool* aConfirm)
{
    UserpassDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxQuestion));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));
    if (aUsername && *aUsername) {
        d.username->setText(QString::fromUcs2(*aUsername));
    }
    if (aPassword && *aPassword) {
        d.password->setText(QString::fromUcs2(*aPassword));
    }
    if (aCheckMsg) {
        d.check->setText(QString::fromUcs2(aCheckMsg));
        d.check->setChecked(*aCheckValue);
    }
    else {
        d.check->hide();
    }
    d.adjustSize();
    int ret = d.exec();

    if (aCheckMsg) {
        *aCheckValue = d.check->isChecked();
    }
    *aConfirm = (ret & QMessageBox::Ok);
    if (*aConfirm) {
        if (*aUsername) nsMemory::Free(*aUsername);
        *aUsername =
            ToNewUnicode(NS_ConvertUTF8toUTF16(d.username->text().utf8()));
        if (*aPassword) nsMemory::Free(*aPassword);
        *aPassword =
            ToNewUnicode(NS_ConvertUTF8toUTF16(d.password->text().utf8()));
    }

    return NS_OK;
}

/**
 * Puts up a dialog with a password field and an optional checkbox.
 *
 * @param dialogTitle
 * @param text
 * @param password      in: Pre-fills the dialog field if non-null
 *                      out: If result is true, a newly allocated
 *                      string. If result is false, in string is not
 *                      touched.
 * @param checkMsg      if null, check box will not be shown
 * @param checkValue
 * @return true for OK, false for Cancel
 */
NS_IMETHODIMP
QtPromptService::PromptPassword(nsIDOMWindow* aParent,
                                const PRUnichar* aDialogTitle,
                                const PRUnichar* aDialogText,
                                PRUnichar** aPassword,
                                const PRUnichar* aCheckMsg,
                                PRBool* aCheckValue,
                                PRBool* aConfirm)
{
    UserpassDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxQuestion));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));
    d.lb_username->hide();
    d.username->hide();
    if (aPassword && *aPassword) {
        d.password->setText(QString::fromUcs2(*aPassword));
    }
    if (aCheckMsg) {
        d.check->setText(QString::fromUcs2(aCheckMsg));
        d.check->setChecked(*aCheckValue);
    }
    else {
        d.check->hide();
    }
    d.adjustSize();
    int ret = d.exec();

    if (aCheckMsg) {
        *aCheckValue = d.check->isChecked();
    }
    *aConfirm = (ret & QMessageBox::Ok);
    if (*aConfirm) {
        if (*aPassword) nsMemory::Free(*aPassword);
        *aPassword =
            ToNewUnicode(NS_ConvertUTF8toUTF16(d.password->text().utf8()));
    }

    return NS_OK;
}

/**
 * Puts up a dialog box which has a list box of strings
 */
NS_IMETHODIMP
QtPromptService::Select(nsIDOMWindow* aParent,
                        const PRUnichar* aDialogTitle,
                        const PRUnichar* aDialogText,
                        PRUint32 aCount,
                        const PRUnichar** aSelectList,
                        PRInt32* outSelection,
                        PRBool* aConfirm)
{
    SelectDialog d(GetQWidgetForDOMWindow(aParent));
    d.icon->setPixmap(QApplication::style().
                      stylePixmap(QStyle::SP_MessageBoxQuestion));
    if (aDialogTitle) {
        d.setCaption(QString::fromUcs2(aDialogTitle));
    }
    d.message->setText(QString::fromUcs2(aDialogText));
    if (aSelectList) {
        QStringList l;
        for (PRUint32 i = 0; i < aCount; ++i) {
            l.append(QString::fromUcs2(aSelectList[i]));
        }
        d.select->clear();
        d.select->insertStringList(l);
    }
    d.adjustSize();
    int ret = d.exec();

    *aConfirm = (ret & QMessageBox::Ok);
    if (*aConfirm) {
        *outSelection = d.select->currentItem();
    }

    return NS_OK;
}

QWidget*
QtPromptService::GetQWidgetForDOMWindow(nsIDOMWindow* aDOMWindow)
{
    nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");

    nsCOMPtr<nsIWebBrowserChrome> chrome;
    wwatch->GetChromeForWindow(aDOMWindow, getter_AddRefs(chrome));
    nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow = do_QueryInterface(chrome);
    QWidget* parentWidget;
    siteWindow->GetSiteWindow((void**)&parentWidget);

    return parentWidget;
}

QString
QtPromptService::GetButtonLabel(PRUint32 aFlags,
                                PRUint32 aPos,
                                const PRUnichar* aStringValue)
{
    PRUint32 posFlag = (aFlags & (255 * aPos)) / aPos;
    switch (posFlag) {
    case BUTTON_TITLE_OK:
        return qApp->translate("QtPromptService", "&OK");
    case BUTTON_TITLE_CANCEL:
        return qApp->translate("QtPromptService", "&Cancel");
    case BUTTON_TITLE_YES:
        return qApp->translate("QtPromptService", "&Yes");
    case BUTTON_TITLE_NO:
        return qApp->translate("QtPromptService", "&No");
    case BUTTON_TITLE_SAVE:
        return qApp->translate("QtPromptService", "&Save");
    case BUTTON_TITLE_DONT_SAVE:
        return qApp->translate("QtPromptService", "&Don't Save");
    case BUTTON_TITLE_REVERT:
        return qApp->translate("QtPromptService", "&Revert");
    case BUTTON_TITLE_IS_STRING:
        return qApp->translate("QtPromptService",
                               QString::fromUcs2(aStringValue));
    case 0:
        return QString::null;
    default:
        NS_WARNING("Unexpected button flags");
        return QString::null;
    }
}
