

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
 */
#include "StdAfx.h"

#include "MozillaBrowser.h"
#include "PromptService.h"

#include "nsCOMPtr.h"

#include "nsIPromptService.h"
#include "nsIFactory.h"
#include "nsIDOMWindow.h"

//*****************************************************************************
// CPromptService
//*****************************************************************************   

class CPromptService: public nsIPromptService
{
public:
    CPromptService();
    virtual ~CPromptService();

    CMozillaBrowser *GetOwningBrowser(nsIDOMWindow *parent);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROMPTSERVICE
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptService, nsIPromptService)

CPromptService::CPromptService()
{
    NS_INIT_REFCNT();
}

CPromptService::~CPromptService()
{
}


CMozillaBrowser *CPromptService::GetOwningBrowser(nsIDOMWindow *parent)
{
    if (parent == nsnull)
    {
        // return the first element from the list if there is one
        if (CMozillaBrowser::sBrowserList.Count() > 0)
        {
            return (CMozillaBrowser *) CMozillaBrowser::sBrowserList[0];
        }
        return NULL;
    }

    // Search for the browser with a content window matching the one provided
    PRInt32 i;
    for (i = 0; i < CMozillaBrowser::sBrowserList.Count(); i++)
    {
        CMozillaBrowser *p = (CMozillaBrowser *) CMozillaBrowser::sBrowserList[i];
        if (p->mWebBrowser)
        {
            nsCOMPtr<nsIDOMWindow> domWindow;
            p->mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
            if (domWindow.get() == parent)
            {
                return p;
            }
        }
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPrompt

NS_IMETHODIMP CPromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        USES_CONVERSION;
        pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
    }
    return NS_OK;
}

NS_IMETHODIMP CPromptService::AlertCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    const PRUnichar *checkMsg, PRBool *checkValue)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        // TODO show dialog with check box
        USES_CONVERSION;
        pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
    }
    return NS_OK;
}

NS_IMETHODIMP CPromptService::Confirm(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        USES_CONVERSION;
        int nAnswer = pOwner->MessageBox(W2T(text), W2T(dialogTitle),
            MB_YESNO | MB_ICONQUESTION);
        *_retval = (nAnswer == IDYES) ? PR_TRUE : PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP CPromptService::ConfirmCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        // TODO show dialog with check box
        USES_CONVERSION;
        int nAnswer = pOwner->MessageBox(W2T(text), W2T(dialogTitle),
            MB_YESNO | MB_ICONQUESTION);
        *_retval = (nAnswer == IDYES) ? PR_TRUE : PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP CPromptService::ConfirmEx(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUint32 buttonFlags,
    const PRUnichar *button0Title,
    const PRUnichar *button1Title,
    const PRUnichar *button2Title,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRInt32 *buttonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::Prompt(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text, PRUnichar **value,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **username, PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::PromptPassword(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::Select(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text, PRUint32 count,
    const PRUnichar **selectList, PRInt32 *outSelection,
    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// CPromptServiceFactory
//*****************************************************************************   

class CPromptServiceFactory : public nsIFactory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    CPromptServiceFactory();
    virtual ~CPromptServiceFactory();
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptServiceFactory, nsIFactory)

CPromptServiceFactory::CPromptServiceFactory()
{
    NS_INIT_ISUPPORTS();
}

CPromptServiceFactory::~CPromptServiceFactory()
{
}

NS_IMETHODIMP CPromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = NULL;  
    CPromptService *inst = new CPromptService;    
    if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = inst->QueryInterface(aIID, aResult);
    if (rv != NS_OK) {  
        // We didn't get the right interface, so clean up  
        delete inst;  
    }  

    return rv;
}

NS_IMETHODIMP CPromptServiceFactory::LockFactory(PRBool lock)
{
    return NS_OK;
}

//*****************************************************************************   

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
    NS_ENSURE_ARG_POINTER(aFactory);
    *aFactory = nsnull;

    CPromptServiceFactory *result = new CPromptServiceFactory;
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aFactory = result;

    return NS_OK;
}
