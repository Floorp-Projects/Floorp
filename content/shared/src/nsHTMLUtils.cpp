/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  Some silly extra stuff that doesn't have anywhere better to go.

 */

#include "nsCOMPtr.h"
#include "nsHTMLUtils.h"
#include "nsIDocument.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static PRBool
IsAscii(const PRUnichar* aString)
{
  for (const PRUnichar* p = aString; *p != 0; ++p) {
    if (*p & ~PRUnichar(0x007f))
      return PR_FALSE;
  }
  return PR_TRUE;
}


nsresult
NS_MakeAbsoluteURIWithCharset(char* *aResult,
                              const nsString& aSpec,
                              nsIDocument* aDocument,
                              nsIURI* aBaseURI,
                              nsIIOService* aIOService,
                              nsICharsetConverterManager* aConvMgr)
{
  // Initialize aResult in case of tragedy
  *aResult = nsnull;

  // Sanity
  NS_PRECONDITION(aBaseURI != nsnull, "no base URI");
  if (! aBaseURI)
    return NS_ERROR_FAILURE;

  // This gets the relative spec after gyrating it through all the
  // necessary encodings and escaping.
  nsCAutoString spec;

  if (IsAscii(aSpec.GetUnicode())) {
    // If it's ASCII, then just copy the characters
    spec.AssignWithConversion(aSpec);
  }
  else {
    // Otherwise, we'll need to use aDocument to cough up a character
    // set converter, and re-encode the relative portion of the URL as
    // 8-bit characters.
    nsCOMPtr<nsIUnicodeEncoder> encoder;

    if (aDocument) {
      nsCOMPtr<nsICharsetConverterManager> convmgr;
      if (aConvMgr) {
        convmgr = aConvMgr;
      }
      else {
        convmgr = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID);
      }

      if (! convmgr)
        return NS_ERROR_FAILURE;

      nsAutoString charSetID;
      aDocument->GetDocumentCharacterSet(charSetID);

      convmgr->GetUnicodeEncoder(&charSetID, getter_AddRefs(encoder));
    }

    if (encoder) {
      // Got the encoder: let's party.
      PRInt32 len = aSpec.Length();
      PRInt32 maxlen;
      encoder->GetMaxLength(aSpec.GetUnicode(), len, &maxlen);

      char buf[64], *p = buf;
      if (maxlen > sizeof(buf) - 1)
        p = new char[maxlen + 1];

      if (! p)
        return NS_ERROR_OUT_OF_MEMORY;

      encoder->Convert(aSpec.GetUnicode(), &len, p, &maxlen);
      encoder->Finish(p, &len);
      p[maxlen] = 0;

      spec = p;

      if (p != buf)
        delete[] p;
    }
    else {
      // No encoder, but we've got non-ASCII data. Let's UTF-8 encode
      // by default.
      spec = NS_ConvertUCS2toUTF8(aSpec.GetUnicode());
    }

    // Now we need to URL-escape the string.  Make sure we've got an
    // nsIIOService.
    nsCOMPtr<nsIIOService> ioservice;
    if (aIOService) {
      ioservice = aIOService;
    }
    else {
      ioservice = do_GetService(kIOServiceCID);
    }

    if (! ioservice)
      return NS_ERROR_FAILURE;

    // XXX andreas.otte has warned that using the nsIIOService::Escape
    // method in this way may be too conservative (e.g., it won't
    // escape a "#" character that appears in a hostname -- does that
    // matter?) But, since there's nothing better, we'll do it...
    static const PRInt32 kEscapeConservatively = nsIIOService::url_Forced - 1;

    // XXX Unfortunately, we can't escape "in place". Maybe the new string
    // APIs will make that better some day.
    nsXPIDLCString escaped;
    ioservice->Escape(spec.GetBuffer(), kEscapeConservatively, getter_Copies(escaped));

    spec = escaped;
  }

  return aBaseURI->Resolve(spec, aResult);
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



