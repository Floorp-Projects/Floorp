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

#include "nsVixenShell.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIComponentManager.h"
#include "prmem.h"

nsVixenShell::nsVixenShell()
{
    NS_INIT_REFCNT();
}

nsVixenShell::~nsVixenShell()
{
}

NS_IMPL_ISUPPORTS1(nsVixenShell, nsIVixenShell);

NS_IMETHODIMP
nsVixenShell::EncodeDocumentToStream(nsIDOMDocument* aDocument,
                                     nsIOutputStream* aOutputStream)
{
    nsresult rv;

    nsCOMPtr<nsIDocument> document(do_QueryInterface(aDocument, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDocumentEncoder> docEncoder(do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &rv));
    if (NS_FAILED(rv)) return rv;

    rv = docEncoder->Init(document, 
                          NS_LITERAL_STRING("text/xml"), 
                          nsIDocumentEncoder::OutputFormatted);
    if (NS_FAILED(rv)) return rv;

    rv = docEncoder->EncodeToStream(aOutputStream);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsVixenShell::EncodeDocumentToString(nsIDOMDocument* aDocument,
                                     PRUnichar** aResult)
{
    nsresult rv;

    *aResult = nsnull;

    nsCOMPtr<nsIDocument> document(do_QueryInterface(aDocument, &rv));
    if (NS_FAILED(rv)) return rv;

    // XXX - This is dependant on a modification I made to my local version of nsLayoutModule.cpp
    //       to add a doc-encoder contractID for "text/xml" serialization. 
    nsCOMPtr<nsIDocumentEncoder> docEncoder(do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &rv));
    if (NS_FAILED(rv)) return rv;

    rv = docEncoder->Init(document, 
                          NS_LITERAL_STRING("text/xml"), 
                          nsIDocumentEncoder::OutputFormatted);
    if (NS_FAILED(rv)) return rv;

    nsAutoString temp;
    rv = docEncoder->EncodeToString(temp);
    if (NS_FAILED(rv)) return rv;

    PRInt32 len = temp.Length() + sizeof('\0');
    if (!len) return rv;
    
    *aResult = (PRUnichar*) PR_Calloc(len, sizeof(PRUnichar));
    *aResult = (PRUnichar*) memcpy(*aResult, temp.get(), sizeof(PRUnichar)*len);

    return NS_OK;
}
