/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsUConvDll.h"
#include "nsEscape.h"
#include "prmem.h"

static NS_DEFINE_CID(kITextToSubURIIID, NS_ITEXTTOSUBURI_IID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

class nsTextToSubURI: public nsITextToSubURI {
    NS_DECL_ISUPPORTS
public:
    nsTextToSubURI();
    virtual ~nsTextToSubURI();
    NS_DECL_NSITEXTTOSUBURI
};

nsTextToSubURI::nsTextToSubURI()
{
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
}
nsTextToSubURI::~nsTextToSubURI()
{
    PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMPL_ISUPPORTS(nsTextToSubURI, kITextToSubURIIID)

NS_IMETHODIMP  nsTextToSubURI::ConvertAndEscape(
  const char *charset, const PRUnichar *text, char **_retval) 
{
  *_retval = nsnull;
  nsAutoString charsetStr(charset);
  nsIUnicodeEncoder *encoder = nsnull;
  nsresult rv = NS_OK;
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    nsCOMTypeInfo<nsICharsetConverterManager>::GetIID(),
                                    (nsISupports**)&ccm);
  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {
     rv = ccm->GetUnicodeEncoder(&charsetStr, &encoder);
     nsServiceManager::ReleaseService( kCharsetConverterManagerCID, ccm);
     rv = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, (PRUnichar)'?');
     if(NS_SUCCEEDED(rv))
     {
        char buf[256];
        char *pBuf = buf;
        PRInt32 ulen = nsCRT::strlen(text);
        PRInt32 outlen = 0;
        if(NS_SUCCEEDED(rv = encoder->GetMaxLength(text, ulen, &outlen))) 
        {
           if(outlen >= 256) {
              pBuf = (char*)PR_Malloc(outlen+1);
           }
           if(nsnull == pBuf) {
              outlen = 255;
              pBuf = buf;
           }
           if(NS_SUCCEEDED(rv = encoder->Convert(text,&ulen, pBuf, &outlen))) {
              pBuf[outlen] = '\0';
              *_retval = nsEscape(pBuf, url_XPAlphas);
              if(nsnull == *_retval)
                rv = NS_ERROR_OUT_OF_MEMORY;
           }
        }
        if(pBuf != buf)
           PR_Free(pBuf);
     }
  }
  
  return rv;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewTextToSubURI(nsISupports* aOuter, 
                   const nsIID &aIID,
                   void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsTextToSubURI* inst = new nsTextToSubURI();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}
