/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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

