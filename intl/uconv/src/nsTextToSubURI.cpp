/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
}
nsTextToSubURI::~nsTextToSubURI()
{
}

NS_IMPL_ISUPPORTS1(nsTextToSubURI, nsITextToSubURI)

NS_IMETHODIMP  nsTextToSubURI::ConvertAndEscape(
  const char *charset, const PRUnichar *text, char **_retval) 
{
  if(nsnull == _retval)
    return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  nsAutoString charsetStr; charsetStr.AssignWithConversion(charset);
  nsIUnicodeEncoder *encoder = nsnull;
  nsresult rv = NS_OK;
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);
  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {
     rv = ccm->GetUnicodeEncoder(&charsetStr, &encoder);
     nsServiceManager::ReleaseService( kCharsetConverterManagerCID, ccm);
     if (NS_SUCCEEDED(rv)) {
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
       NS_IF_RELEASE(encoder);
     }
  }
  
  return rv;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeAndConvert(
  const char *charset, const char *text, PRUnichar **_retval) 
{
  if(nsnull == _retval)
    return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  nsresult rv = NS_OK;
  
  // unescape the string, unescape changes the input
  char *unescaped = nsCRT::strdup((char *) text);
  if (nsnull == unescaped)
    return NS_ERROR_OUT_OF_MEMORY;
  unescaped = nsUnescape(unescaped);
  NS_ASSERTION(unescaped, "nsUnescape returned null");

  // Convert from the charset to unicode
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(kCharsetConverterManagerCID, &rv); 
  if (NS_SUCCEEDED(rv)) {
    nsAutoString charsetStr; charsetStr.AssignWithConversion(charset);
    nsIUnicodeDecoder *decoder;
    rv = ccm->GetUnicodeDecoder(&charsetStr, &decoder);
    if (NS_SUCCEEDED(rv)) {
      PRUnichar *pBuf = nsnull;
      PRInt32 len = nsCRT::strlen(unescaped);
      PRInt32 outlen = 0;
      if (NS_SUCCEEDED(rv = decoder->GetMaxLength(unescaped, len, &outlen))) {
        pBuf = (PRUnichar *) PR_Malloc((outlen+1)*sizeof(PRUnichar*));
        if (nsnull == pBuf)
          rv = NS_ERROR_OUT_OF_MEMORY;
        else {
          if (NS_SUCCEEDED(rv = decoder->Convert(unescaped, &len, pBuf, &outlen))) {
            pBuf[outlen] = 0;
            *_retval = pBuf;
          }
        }
      }
      NS_IF_RELEASE(decoder);
    }
  }
  PR_FREEIF(unescaped);

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
