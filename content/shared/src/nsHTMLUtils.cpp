/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*

  Some silly extra stuff that doesn't have anywhere better to go.

 */

#include "nsCOMPtr.h"
#include "nsHTMLUtils.h"
#include "nsIDocument.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

nsresult
NS_MakeAbsoluteURIWithCharset(nsACString &aResult,
                              const nsString& aSpec,
                              nsIDocument* aDocument,
                              nsIURI* aBaseURI,
                              nsIIOService* aIOService,
                              nsICharsetConverterManager* aConvMgr)
{
  // Initialize aResult in case of tragedy
  aResult.Truncate();

  // Sanity
  NS_PRECONDITION(aBaseURI != nsnull, "no base URI");
  if (! aBaseURI)
    return NS_ERROR_FAILURE;

  // This gets the relative spec after gyrating it through all the
  // necessary encodings and escaping.

  if (IsASCII(aSpec)) {
    // If it's ASCII, then just copy the characters
    return aBaseURI->Resolve(NS_LossyConvertUCS2toASCII(aSpec), aResult);
  }

  nsCOMPtr<nsIURI> absURI;
  nsresult rv;

  nsAutoString originCharset; // XXX why store charset as UCS2?
  if (aDocument && NS_FAILED(aDocument->GetDocumentCharacterSet(originCharset)))
    originCharset.Truncate();

  // URI can't be encoded in UTF-16, UTF-16BE, UTF-16LE, UTF-32, UTF-32-LE,
  // UTF-32LE, UTF-32BE (yet?). Truncate it and leave it to default (UTF-8)
  if (originCharset[0] == 'U' &&
      originCharset[1] == 'T' &&
      originCharset[2] == 'F')
    originCharset.Truncate();

  rv = nsHTMLUtils::IOService->NewURI(NS_ConvertUCS2toUTF8(aSpec),
                                      NS_LossyConvertUCS2toASCII(originCharset).get(),
                                      aBaseURI, getter_AddRefs(absURI));
  if (NS_FAILED(rv)) return rv; 

  return absURI->GetSpec(aResult);
}


nsIIOService* nsHTMLUtils::IOService;
nsICharsetConverterManager* nsHTMLUtils::CharsetMgr;
PRInt32 nsHTMLUtils::gRefCnt;

void
nsHTMLUtils::AddRef()
{
  if (++gRefCnt == 1) {
    nsServiceManager::GetService(kIOServiceCID, NS_GET_IID(nsIIOService),
                                 NS_REINTERPRET_CAST(nsISupports**, &IOService));

    nsServiceManager::GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID,
                                 NS_GET_IID(nsICharsetConverterManager),
                                 NS_REINTERPRET_CAST(nsISupports**, &CharsetMgr));
  }
}


void
nsHTMLUtils::Release()
{
  if (--gRefCnt == 0) {
    nsServiceManager::ReleaseService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, CharsetMgr);
    nsServiceManager::ReleaseService(kIOServiceCID, IOService);
  }
}



