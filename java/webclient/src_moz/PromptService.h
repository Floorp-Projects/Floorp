/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Kyle Yuan <kyle.yuan@sun.com>
 *
 * Contributor(s):
 *
 */

#ifndef __PromptService_h
#define __PromptService_h

#include "nsCOMPtr.h"
#include "nsIPromptService.h"
#include "nsIDOMWindow.h"
#include "nsIWindowWatcher.h"

//*****************************************************************************
// PromptService
//*****************************************************************************   

class CBrowserContainer;

class PromptService: public nsIPromptService
{
public:
                PromptService();
    virtual    ~PromptService();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROMPTSERVICE

private:
    nsCOMPtr<nsIWindowWatcher> mWWatch;

    CBrowserContainer *BrowserContainerForDOMWindow(nsIDOMWindow *aWindow);

    nsresult PromptUniversalDialog(
        nsIDOMWindow *parent,
        const PRUnichar *dialogTitle,       //[in] dialog title
        const PRUnichar *text,              //[in] text main message for dialog
        const PRUnichar *checkMsg,          //[in] message for checkbox
        const PRUnichar *button0Title,      //[in] text for first button
        const PRUnichar *button1Title,      //[in] text for second button
        const PRUnichar *button2Title,      //[in] text for third button
        const PRUnichar *button3Title,      //[in] text for fourth button
        const PRUnichar *editfield1Msg,     //[in] message for first edit field
        const PRUnichar *editfield2Msg,     //[in] message for second edit field
        PRUnichar **editfield1Value,        //[in/out] initial/return value for first edit field
        PRUnichar **editfield2Value,        //[in/out] initial/return value for second edit field
        PRInt32 numButtons,                 //[in] total number of buttons (0 to 4)
        PRInt32 numFields,                  //[in] total number of edit fields (0 to 2)
        PRInt32 fieldIsPasswd,              //[in] whether or not editField1 is a password
        PRBool *checkState,                 //[out] state for checkbox
        PRInt32 *buttonPressed              //[out] which button was pressed
        );
};

#endif
