/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
CBrowserImpl::ConfirmEx(const PRUnichar *dialogTitle, const PRUnichar *text,
                        PRUint32 buttonFlags, const PRUnichar *button0Title,
                        const PRUnichar *button1Title, const PRUnichar *button2Title,
                        const PRUnichar *checkMsg, PRBool *checkValue,
                        PRInt32 *buttonPressed)
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

