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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsNonPersistAuthPrompt.h"

#include "nsReadableUtils.h"

//*****************************************************************************
// nsNonPersistAuthPrompt
//*****************************************************************************   

nsNonPersistAuthPrompt::nsNonPersistAuthPrompt()
{
    NS_INIT_REFCNT();
}

nsNonPersistAuthPrompt::~nsNonPersistAuthPrompt()
{
}

NS_IMPL_ISUPPORTS2(nsNonPersistAuthPrompt, nsISingleSignOnPrompt, nsIAuthPrompt)

//*****************************************************************************
// nsNonPersistAuthPrompt::nsISingleSignOnPrompt
//*****************************************************************************   

NS_IMETHODIMP nsNonPersistAuthPrompt::SetPromptDialogs(nsIPrompt *dialogs)
{
    mPromptDialogs = dialogs;
    return NS_OK;
}


//*****************************************************************************
// nsNonPersistAuthPrompt::nsIAuthPrompt
//*****************************************************************************   

NS_IMETHODIMP nsNonPersistAuthPrompt::Prompt(const PRUnichar *dialogTitle,
                                             const PRUnichar *text,
                                             const PRUnichar *passwordRealm,
                                             PRUint32 savePassword,
                                             const PRUnichar *defaultText,
                                             PRUnichar **result,
                                             PRBool *_retval)
{
    NS_ENSURE_TRUE(mPromptDialogs, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_ARG_POINTER(result);
    
    *result = ToNewUnicode(nsDependentString(defaultText));
    if (*result == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    return mPromptDialogs->Prompt(dialogTitle, text, result, nsnull, nsnull, _retval);
}

NS_IMETHODIMP nsNonPersistAuthPrompt::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                                                const PRUnichar *text,
                                                                const PRUnichar *passwordRealm,
                                                                PRUint32 savePassword,
                                                                PRUnichar **user,
                                                                PRUnichar **pwd,
                                                                PRBool *_retval)
{
    NS_ENSURE_TRUE(mPromptDialogs, NS_ERROR_NOT_INITIALIZED);

    return mPromptDialogs->PromptUsernameAndPassword(dialogTitle, text, user, pwd, nsnull, nsnull, _retval);
}

NS_IMETHODIMP nsNonPersistAuthPrompt::PromptPassword(const PRUnichar *dialogTitle,
                                                     const PRUnichar *text,
                                                     const PRUnichar *passwordRealm,
                                                     PRUint32 savePassword,
                                                     PRUnichar **pwd,
                                                     PRBool *_retval)
{
    NS_ENSURE_TRUE(mPromptDialogs, NS_ERROR_NOT_INITIALIZED);

    return mPromptDialogs->PromptPassword(dialogTitle, text, pwd, nsnull, nsnull, _retval);
}
