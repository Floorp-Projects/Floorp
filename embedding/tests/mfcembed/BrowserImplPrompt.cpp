/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

// File overview.....
//
// The nsIPrompt interface is mainly used to convey/get information
// from a user via Alerts, Prompts etc.
// 

#ifdef _WINDOWS
  #include "stdafx.h"
#endif
#include "BrowserImpl.h"
#include "IBrowserFrameGlue.h"

//*****************************************************************************
// CBrowserImpl::nsIPrompt Implementation
//*****************************************************************************   


// Needed for JavaScript and other cases where a msg needs to be 
// shown to the user. For ex, when a page has some JS such as
// "Alert("Hello")" this method will be invoked
NS_IMETHODIMP
CBrowserImpl::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;
	
	m_pBrowserFrameGlue->Alert(dialogTitle, text);

	return NS_OK;
}

// Invoked in the case of a JS confirm() method invocation
NS_IMETHODIMP
CBrowserImpl::Confirm(const PRUnichar *dialogTitle, 
			   const PRUnichar *text, PRBool *retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->Confirm(dialogTitle, text, retval);

	return NS_OK;
}

NS_IMETHODIMP
CBrowserImpl::Prompt(const PRUnichar *dialogTitle, const PRUnichar *text,
                     PRUnichar **promptText,
                     const PRUnichar *checkMsg, PRBool *checkValue,
                     PRBool *_retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->Prompt(dialogTitle, text, promptText, checkMsg, checkValue, _retval);

	return NS_OK;
}

NS_IMETHODIMP
CBrowserImpl::PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text,
                             PRUnichar **password,
                             const PRUnichar *checkMsg, PRBool *checkValue,
                             PRBool *_retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->PromptPassword(dialogTitle, text, password,
	                                    checkMsg, checkValue, _retval);

    return NS_OK;
}

NS_IMETHODIMP
CBrowserImpl::PromptUsernameAndPassword(const PRUnichar *dialogTitle, const PRUnichar *text,
                                        PRUnichar **username, PRUnichar **password,
                                        const PRUnichar *checkMsg, PRBool *checkValue,
                                        PRBool *_retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->PromptUserNamePassword(dialogTitle, text, 
							                    username, password,
							                    checkMsg, checkValue, 
							                    _retval);

    return NS_OK;
}

// This method is evil/painful. It will be gone when bug 46859 is done
//
// So far these are the cases in which UniversalDialog seems to be
// getting called when we wrap our nsIPrompt with nsISingleSignOnPrompt
// (There are other cases in which this method may be called, 
//  but, i'm covering the most frequent cases for now - assuming that
//  we'll do away with this method soon)
//
//		- JavaScript's Prompt() method - Should be calling nsIPrompt::Prompt()
//										 instead
//
//		- To get UserName/Password while accessing a password protected site via
//		  HTTP - They should be calling nsIPrompt::PromptUserNameAndPassword() instead
//		  (However, the current nsIPrompt::PromptUserNameAndPassword() is inadequate
//	       in the sense that it does not have a way to specify the CheckBox text
//		   and the CheckState return value. So this issue also needs to be addressed
//			before getting rid of the Universal Dialog)
//
//		- To get just a password in the case the username's already known/specified
//		  i.e. in cases like ftp://chak@server.domain.com
//		  (Note that the user name "chak" is specified but the password is not
//		  They should be calling our nsIPrompt::PromptPassword() instead
//		  (However, the current nsIPrompt::PromptPassword() is inadequate
//	       in the sense that it does not have a way to specify the CheckBox text
//		   and the CheckState return value. So this issue also needs to be addressed
//		   before getting rid of the Universal Dialog)
//
// Since this method might go away here's how i'm currently using it:
// 
//  Identify if this method is called on behalf of a JS Prompt() etc. and if so
//  delegate it to our appropriate implementations
//  When this method's axed the correct IPrompt methods will be called and
//  we can just get rid of this one
//
NS_IMETHODIMP
CBrowserImpl::UniversalDialog(const PRUnichar *titleMessage,
				   const PRUnichar *dialogTitle,
				   const PRUnichar *text,
				   const PRUnichar *checkboxMsg,
				   const PRUnichar *button0Text, 
				   const PRUnichar *button1Text,
				   const PRUnichar *button2Text, 
				   const PRUnichar *button3Text,
				   const PRUnichar *editfield1Msg,
				   const PRUnichar *editfield2Msg,
				   PRUnichar **editfield1Value, 
				   PRUnichar **editfield2Value,
				   const PRUnichar *iconURL,
				   PRBool *checkboxState, 
				   PRInt32 numberButtons, 
				   PRInt32 numberEditfields,
				   PRInt32 editField1Password,
				   PRInt32 *buttonPressed)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;
		
	PRBool confirmed;

	if(numberEditfields == 1 && checkboxMsg == NULL && editField1Password == 0)
	{
		// This is a Prompt()

		m_pBrowserFrameGlue->Prompt(dialogTitle, text, editfield1Value, NULL, NULL, &confirmed);

		// The Prompt() methods return PR_TRUE/PR_FALSE depending on whether 
		// the OK/CANCEL was pressed. However, UniversalDialog checks
		// for the index of the button pressed i.e. "0" for the "OK" button and
		// "1" for the CANCEL button. 

		// So, now let's translate to what the UniversalDlg expects

		if(confirmed) //Will be TRUE i.e. 1 if the user chose OK
			*buttonPressed = 0; //Set it to the OK button index i.e. to "0"
		else
			*buttonPressed = 1; //Set it to the Cancel button index i.e. to "1"

		return NS_OK;
	}
	else if(numberEditfields == 1 && checkboxMsg != NULL && editField1Password == 1)
	{
		// This is a PromptPassword()

		m_pBrowserFrameGlue->PromptPassword(dialogTitle, text, editfield1Value,
		                                    checkboxMsg, checkboxState, &confirmed);

		//See comments above on why we're doing this...
		if(confirmed) 
			*buttonPressed = 0;
		else
			*buttonPressed = 1;

		return NS_OK;
	}
	else if(numberEditfields == 2 && checkboxMsg != NULL && editField1Password == 0)
	{
		// This is a username/password dialog

		m_pBrowserFrameGlue->PromptUserNamePassword(dialogTitle, text,
		                        editfield1Value, editfield2Value, 
								checkboxMsg, checkboxState, 
								&confirmed);

		//See comments above on why we're doing this...
		if(confirmed)
			*buttonPressed = 0;
		else
			*buttonPressed = 1;

		return NS_OK;
	}
	else
		return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CBrowserImpl::AlertCheck(const PRUnichar *dialogTitle, 
			      const PRUnichar *text, 
			      const PRUnichar *checkMsg,
			      PRBool *checkValue)
{
	    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CBrowserImpl::ConfirmCheck(const PRUnichar *dialogTitle,
				const PRUnichar *text,
				const PRUnichar *checkMsg, 
				PRBool *checkValue, PRBool *retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CBrowserImpl::Select(const PRUnichar *dialogTitle,
			  const PRUnichar *text, PRUint32 count,
			  const PRUnichar **selectList,
			  PRInt32 *outSelection, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

