/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "GtkPromptService.h"
#include "EmbedPrompter.h"
#include <nsString.h>
#include <nsIWindowWatcher.h>
#include <nsIWebBrowserChrome.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsCOMPtr.h>
#include <nsIServiceManager.h>

GtkPromptService::GtkPromptService()
{
}

GtkPromptService::~GtkPromptService()
{
}

NS_IMPL_ISUPPORTS1(GtkPromptService, nsIPromptService)

NS_IMETHODIMP
GtkPromptService::Alert(nsIDOMWindow* aParent, const PRUnichar* aDialogTitle, 
                        const PRUnichar* aDialogText)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Alert").get());
    prompter.SetMessageText(aDialogText);
    prompter.Create(EmbedPrompter::TYPE_ALERT, 
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::AlertCheck(nsIDOMWindow* aParent,
                             const PRUnichar* aDialogTitle,
                             const PRUnichar* aDialogText,
                             const PRUnichar* aCheckMsg, PRBool* aCheckValue)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Alert").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetCheckMessage(aCheckMsg);
    prompter.SetCheckValue(*aCheckValue);
    prompter.Create(EmbedPrompter::TYPE_ALERT_CHECK,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    prompter.GetCheckValue(aCheckValue);
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::Confirm(nsIDOMWindow* aParent,
                          const PRUnichar* aDialogTitle,
                          const PRUnichar* aDialogText, PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Confirm").get());
    prompter.SetMessageText(aDialogText);
    prompter.Create(EmbedPrompter::TYPE_CONFIRM,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    prompter.GetConfirmValue(aConfirm);
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::ConfirmCheck(nsIDOMWindow* aParent,
                               const PRUnichar* aDialogTitle,
                               const PRUnichar* aDialogText,
                               const PRUnichar* aCheckMsg,
                               PRBool* aCheckValue, PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Confirm").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetCheckMessage(aCheckMsg);
    prompter.SetCheckValue(*aCheckValue);
    prompter.Create(EmbedPrompter::TYPE_CONFIRM_CHECK,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    prompter.GetCheckValue(aCheckValue);
    prompter.GetConfirmValue(aConfirm);
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::ConfirmEx(nsIDOMWindow* aParent,
                            const PRUnichar* aDialogTitle,
                            const PRUnichar* aDialogText,
                            PRUint32 aButtonFlags,
                            const PRUnichar* aButton0Title,
                            const PRUnichar* aButton1Title,
                            const PRUnichar* aButton2Title,
                            const PRUnichar* aCheckMsg, PRBool* aCheckValue,
                            PRInt32* aRetVal)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Confirm").get());
    prompter.SetMessageText(aDialogText);

    nsAutoString button0Label, button1Label, button2Label;
    GetButtonLabel(aButtonFlags, BUTTON_POS_0, aButton0Title, button0Label);
    GetButtonLabel(aButtonFlags, BUTTON_POS_1, aButton1Title, button1Label);
    GetButtonLabel(aButtonFlags, BUTTON_POS_2, aButton2Title, button2Label);
    prompter.SetButtons(button0Label.get(), button1Label.get(),
                        button2Label.get());

    if (aCheckMsg)
        prompter.SetCheckMessage(aCheckMsg);
    if (aCheckValue)
        prompter.SetCheckValue(*aCheckValue);

    prompter.Create(EmbedPrompter::TYPE_UNIVERSAL,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();

    if (aCheckValue)
        prompter.GetCheckValue(aCheckValue);

    prompter.GetButtonPressed(aRetVal);

    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::Prompt(nsIDOMWindow* aParent, const PRUnichar* aDialogTitle,
                         const PRUnichar* aDialogText, PRUnichar** aValue,
                         const PRUnichar* aCheckMsg, PRBool* aCheckValue,
                         PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Prompt").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetTextValue(*aValue);
    if (aCheckMsg) {
        prompter.SetCheckMessage(aCheckMsg);
        prompter.SetCheckValue(*aCheckValue);
    }
    prompter.Create(EmbedPrompter::TYPE_PROMPT,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    if (aCheckValue)
        prompter.GetCheckValue(aCheckValue);
    prompter.GetConfirmValue(aConfirm);
    if (aConfirm) {
        if (*aValue)
            nsMemory::Free(*aValue);
        prompter.GetTextValue(aValue);
    }
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::PromptUsernameAndPassword(nsIDOMWindow* aParent,
                                            const PRUnichar* aDialogTitle,
                                            const PRUnichar* aDialogText,
                                            PRUnichar** aUsername,
                                            PRUnichar** aPassword,
                                            const PRUnichar* aCheckMsg,
                                            PRBool* aCheckValue,
                                            PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Prompt").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetUser(*aUsername);
    prompter.SetPassword(*aPassword);
    if (aCheckMsg) {
        prompter.SetCheckMessage(aCheckMsg);
        prompter.SetCheckValue(*aCheckValue);
    }
    prompter.Create(EmbedPrompter::TYPE_PROMPT_USER_PASS,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    if (aCheckValue)
        prompter.GetCheckValue(aCheckValue);
    prompter.GetConfirmValue(aConfirm);
    if (*aConfirm) {
        if (*aUsername)
            nsMemory::Free(*aUsername);
        prompter.GetUser(aUsername);

        if (*aPassword)
            nsMemory::Free(*aPassword);
        prompter.GetPassword(aPassword);
    }
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::PromptPassword(nsIDOMWindow* aParent,
                                 const PRUnichar* aDialogTitle,
                                 const PRUnichar* aDialogText,
                                 PRUnichar** aPassword,
                                 const PRUnichar* aCheckMsg,
                                 PRBool* aCheckValue, PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Prompt").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetPassword(*aPassword);
    if (aCheckMsg) {
        prompter.SetCheckMessage(aCheckMsg);
        prompter.SetCheckValue(*aCheckValue);
    }
    prompter.Create(EmbedPrompter::TYPE_PROMPT_PASS,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    if (aCheckValue)
        prompter.GetCheckValue(aCheckValue);
    prompter.GetConfirmValue(aConfirm);
    if (*aConfirm) {
        if (*aPassword)
            nsMemory::Free(*aPassword);
        prompter.GetPassword(aPassword);
    }
    return NS_OK;
}

NS_IMETHODIMP
GtkPromptService::Select(nsIDOMWindow* aParent, const PRUnichar* aDialogTitle,
                         const PRUnichar* aDialogText, PRUint32 aCount,
                         const PRUnichar** aSelectList, PRInt32* outSelection,
                         PRBool* aConfirm)
{
    EmbedPrompter prompter;
    prompter.SetTitle(aDialogTitle ? aDialogTitle : NS_LITERAL_STRING("Select").get());
    prompter.SetMessageText(aDialogText);
    prompter.SetItems(aSelectList, aCount);
    prompter.Create(EmbedPrompter::TYPE_SELECT,
                    GetGtkWindowForDOMWindow(aParent));
    prompter.Run();
    prompter.GetSelectedItem(outSelection);
    prompter.GetConfirmValue(aConfirm);
    return NS_OK;
}

GtkWindow*
GtkPromptService::GetGtkWindowForDOMWindow(nsIDOMWindow* aDOMWindow)
{
    nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");

    if (!aDOMWindow)
        return NULL;

    nsCOMPtr<nsIWebBrowserChrome> chrome;
    wwatch->GetChromeForWindow(aDOMWindow, getter_AddRefs(chrome));
    nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow = do_QueryInterface(chrome);

    if (!siteWindow)
        return NULL;

    GtkWidget* parentWidget;
    siteWindow->GetSiteWindow((void**)&parentWidget);

    if (!parentWidget)
        return NULL;

    GtkWidget* gtkWin = gtk_widget_get_toplevel(parentWidget);
    if (GTK_WIDGET_TOPLEVEL(gtkWin))
        return GTK_WINDOW(gtkWin);

    return NULL;
}

void
GtkPromptService::GetButtonLabel(PRUint32 aFlags, PRUint32 aPos,
                                 const PRUnichar* aStringValue,
                                 nsAString& aLabel)
{
    PRUint32 posFlag = (aFlags & (255 * aPos)) / aPos;
    switch (posFlag) {
    case BUTTON_TITLE_OK:
        aLabel.AssignLiteral(GTK_STOCK_OK);
        break;
    case BUTTON_TITLE_CANCEL:
        aLabel.AssignLiteral(GTK_STOCK_CANCEL);
        break;
    case BUTTON_TITLE_YES:
        aLabel.AssignLiteral(GTK_STOCK_YES);
        break;
    case BUTTON_TITLE_NO:
        aLabel.AssignLiteral(GTK_STOCK_NO);
        break;
    case BUTTON_TITLE_SAVE:
        aLabel.AssignLiteral(GTK_STOCK_SAVE);
        break;
    case BUTTON_TITLE_DONT_SAVE:
        aLabel.AssignLiteral("Don't Save");
        break;
    case BUTTON_TITLE_REVERT:
        aLabel.AssignLiteral("Revert");
        break;
    case BUTTON_TITLE_IS_STRING:
        aLabel = aStringValue;
        break;
    default:
        NS_WARNING("Unexpected button flags");
    }
}
