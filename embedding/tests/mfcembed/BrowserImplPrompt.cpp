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
// There are two distinct scenarious to be aware of when implementing this
// interface:
//   1. When we want to support the single sign-on feature i.e. we're
//	    "wrap"ing our nsIPrompt interface with nsISingleSignOnPrompt
//		(See BrowserImpl::GetInterface() on how we wrap it with
//		 single sign-on)
//   2. When we do not want single sign-on support or that support
//      is missing, for ex, not installed or disabled
//
// Depending on which of the above two scenarios we're operating on
// different methods in this interface are called
// 
// When operating *with* Single sign-on support:
//	nsIPrompt::Alert()		- is called to show an alert
//	nsIPromtp::Confirm()	- is called to show a confirmation msg
//	nsIPrompt::UniversalDlg() - is called for getting a password, getting
//								username/password and to Prompt the user
//								for some information, other cases??
//
// When operating *without* Single sign-on support:
//	nsIPrompt::Alert()		- is called to show an alert
//	nsIPromtp::Confirm()	- is called to show a confirmation msg
//	nsIPromtp::Prompt()		- is called to prompt the user for some info.
//	nsIPromtp::PromptPassword() - is called to get the password
//	nsIPromtp::PromptUsernameAndPassword() - is called to get username/password
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
// (This method will get called whether or not we're wrapping
// our nsIPrompt with nsISingleSignonPrompt)
NS_IMETHODIMP
CBrowserImpl::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;
	
	m_pBrowserFrameGlue->Alert(dialogTitle, text);

	return NS_OK;
}

// Invoked in the case of a JS confirm() method invocation
// (This method will get called whether or not we're wrapping
// our nsIPrompt with nsISingleSignonPrompt)
NS_IMETHODIMP
CBrowserImpl::Confirm(const PRUnichar *dialogTitle, 
			   const PRUnichar *text, PRBool *retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->Confirm(dialogTitle, text, retval);

	return NS_OK;
}

// (This method will get called when we're *NOT* wrapping
// our nsIPrompt with nsISingleSignonPrompt
// However, when we're wrapping our nsIPrompt with
// single sign-on then the UniversalDialog is being
// invoked to put up a prompt.
NS_IMETHODIMP
CBrowserImpl::Prompt(const PRUnichar *dialogTitle,
			  const PRUnichar *text,
			  const PRUnichar *passwordRealm,
			  PRUint32 savePassword,
			  const PRUnichar *defaultText, 
			  PRUnichar **result, PRBool *retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->Prompt(dialogTitle, text, defaultText, result, retval);

	return NS_OK;
}

NS_IMETHODIMP
CBrowserImpl::PromptPassword(const PRUnichar *dialogTitle,
				  const PRUnichar *text, 
				  const PRUnichar *passwordRealm,
				  PRUint32 savePassword, PRUnichar **pwd,
				  PRBool *retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	// Note that we're using the same PromptPassword method in 
	// IBrowserFrameGlue for both the single sign-on and non-
	// single sign-on cases. 
	// In the non-single sign-on case i.e. here, we do not
	// have a checkboxmsg or chkboxstate to worry about - 
	// hence NULLs are being passed for those params below

	m_pBrowserFrameGlue->PromptPassword(dialogTitle, text, NULL/*no chkbox msg*/, 
							NULL/*no chkboxState*/, pwd, retval);

    return NS_OK;
}

NS_IMETHODIMP
CBrowserImpl::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
					     const PRUnichar *text,
					     const PRUnichar *passwordRealm,
					     PRUint32 savePassword, 
					     PRUnichar **user,
					     PRUnichar **pwd, 
					     PRBool *retval)
{
	if(! m_pBrowserFrameGlue)
		return NS_ERROR_FAILURE;

	m_pBrowserFrameGlue->PromptUserNamePassword(dialogTitle, text, 
							NULL/*UserName Label*/, NULL/*Password Label*/, 
							NULL/*checkboxMsg*/, NULL/*checkboxState*/,
							user, pwd, 
							retval);

    return NS_OK;
}

// This method is evil/painful. This needs to go away and i think there's
// already a bug w.r.t this
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

	if(numberEditfields == 1 && checkboxMsg == NULL && editField1Password == 0)
	{
		// This is a Prompt()

		m_pBrowserFrameGlue->Prompt(dialogTitle, text, *editfield1Value, editfield1Value, buttonPressed);

		// The Prompt() methods return PR_TRUE/PR_FALSE depending on whether 
		// the OK/CANCEL was pressed. However, UniversalDialog checks
		// for the index of the button pressed i.e. "0" for the "OK" button and
		// "1" for the CANCEL button. 

		// So, now let's translate to what the UniversalDlg expects

		if(*buttonPressed) //Will be TRUE i.e. 1 if the user chose OK
			*buttonPressed = 0; //Set it to the OK button index i.e. to "0"
		else
			*buttonPressed = 1; //Set it to the Cancel button index i.e. to "1"

		return NS_OK;
	}
	else if(numberEditfields == 1 && checkboxMsg != NULL && editField1Password == 1)
	{
		// This is a PromptPassword()

		m_pBrowserFrameGlue->PromptPassword(dialogTitle, text, checkboxMsg, 
								checkboxState, editfield1Value, buttonPressed);

		//See comments above on why we're doing this...
		if(*buttonPressed) 
			*buttonPressed = 0;
		else
			*buttonPressed = 1;

		return NS_OK;
	}
	else if(numberEditfields == 2 && checkboxMsg != NULL && editField1Password == 0)
	{
		// This is a username/password dialog

		m_pBrowserFrameGlue->PromptUserNamePassword(dialogTitle, text, 
								editfield1Msg, editfield2Msg, 
								checkboxMsg, checkboxState,
								editfield1Value, editfield2Value, 
								buttonPressed);

		//See comments above on why we're doing this...
		if(*buttonPressed)
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

