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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include "nsWebBrowserFind.h"

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsITextServicesDocument.h"
#include "nsTextServicesCID.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"


static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);

//*****************************************************************************
// nsWebBrowserFindImpl
//*****************************************************************************   


nsWebBrowserFindImpl::nsWebBrowserFindImpl() :
    mFindBackwards(PR_FALSE), mWrapFind(PR_FALSE),
    mEntireWord(PR_FALSE), mMatchCase(PR_FALSE)
{
}

nsWebBrowserFindImpl::~nsWebBrowserFindImpl()
{
}

nsresult nsWebBrowserFindImpl::Init()
{
    nsresult rv;
    mTSFind = do_CreateInstance(NS_FINDANDREPLACE_CONTRACTID, &rv);
    return rv;
}

nsresult nsWebBrowserFindImpl::SetSearchString(const PRUnichar* aString)
{
    mSearchString = aString;
    return NS_OK;
}

nsresult nsWebBrowserFindImpl::GetSearchString(PRUnichar** aString)
{
    NS_ENSURE_ARG_POINTER(aString);
    *aString = mSearchString.ToNewUnicode();
    return *aString ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsWebBrowserFindImpl::DoFind(nsIDOMWindow* aWindow, PRBool* aDidFind)
{
    NS_ENSURE_TRUE(mTSFind, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    nsCOMPtr<nsITextServicesDocument> txtDoc;
    rv = MakeTSDocument(aWindow, getter_AddRefs(txtDoc));
    if (NS_FAILED(rv) || !txtDoc)
        return rv;
    
    (void) mTSFind->SetCaseSensitive(mMatchCase);
    (void) mTSFind->SetFindBackwards(mFindBackwards);
    (void) mTSFind->SetWrapFind(mWrapFind);
    (void) mTSFind->SetEntireWord(mEntireWord);

    rv = mTSFind->SetTsDoc(txtDoc);
    if (NS_FAILED(rv))
        return rv;

    rv =  mTSFind->Find(mSearchString.GetUnicode(), aDidFind);

    mTSFind->SetTsDoc(nsnull);

    return rv;
}

nsresult nsWebBrowserFindImpl::MakeTSDocument(nsIDOMWindow* aWindow, nsITextServicesDocument** aDoc)
{
    NS_ENSURE_ARG(aWindow);
    NS_ENSURE_ARG_POINTER(aDoc);

    nsresult rv;
    *aDoc = NULL;

    nsCOMPtr<nsITextServicesDocument>  tempDoc(do_CreateInstance(kCTextServicesDocumentCID, &rv));
    if (NS_FAILED(rv) || !tempDoc)
        return rv;

    nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(aWindow, &rv);
    if (NS_FAILED(rv) || !globalObj)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> docShell;
    globalObj->GetDocShell(getter_AddRefs(docShell));
    if (!docShell)
        return NS_ERROR_FAILURE;
	
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (!presShell)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> domDoc;    
    rv = aWindow->GetDocument(getter_AddRefs(domDoc));
    if (!domDoc)
        return NS_ERROR_FAILURE;

    rv = tempDoc->InitWithDocument(domDoc, presShell);
    if (NS_FAILED(rv))
        return rv;

    // Return the resulting text services document.
    *aDoc = tempDoc;
    NS_IF_ADDREF(*aDoc);
  
    return rv;
}
