/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
