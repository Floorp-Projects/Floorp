/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
#include "nsReadableUtils.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsUConvDll.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsTextToSubURI.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

nsTextToSubURI::nsTextToSubURI()
{
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
  nsIUnicodeEncoder *encoder = nsnull;
  nsresult rv = NS_OK;
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);
  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {
     rv = ccm->GetUnicodeEncoder(charset, &encoder);
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
             PRInt32 bufLen = outlen;
             if(NS_SUCCEEDED(rv = encoder->Convert(text,&ulen, pBuf, &outlen))) {
                // put termination characters (e.g. ESC(B of ISO-2022-JP) if necessary
                PRInt32 finLen = bufLen - outlen;
                if (finLen > 0) {
                  if (NS_SUCCEEDED(encoder->Finish((char *)(pBuf+outlen), &finLen)))
                    outlen += finLen;
                }
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
    nsIUnicodeDecoder *decoder;
    rv = ccm->GetUnicodeDecoder(charset, &decoder);
    if (NS_SUCCEEDED(rv)) {
      PRUnichar *pBuf = nsnull;
      PRInt32 len = strlen(unescaped);
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

static PRBool statefulCharset(const char *charset)
{
  if (!nsCRT::strncasecmp(charset, "ISO-2022-", sizeof("ISO-2022-")-1) ||
      !nsCRT::strcasecmp(charset, "UTF-7") ||
      !nsCRT::strcasecmp(charset, "HZ-GB-2312"))
    return PR_TRUE;

  return PR_FALSE;
}

nsresult nsTextToSubURI::convertURItoUnicode(const nsAFlatCString &aCharset,
                                             const nsAFlatCString &aURI, 
                                             PRBool aIRI, 
                                             nsAString &_retval)
{
  nsresult rv = NS_OK;

  // check for 7bit encoding the data may not be ASCII after we decode
  PRBool isStatefulCharset = statefulCharset(aCharset.get());

  if (!isStatefulCharset && IsASCII(aURI)) {
    CopyASCIItoUTF16(aURI, _retval);
    return rv;
  }

  if (!isStatefulCharset && aIRI) {
    if (IsUTF8(aURI)) {
      CopyUTF8toUTF16(aURI, _retval);
      return rv;
    }
  }

  // empty charset could indicate UTF-8, but aURI turns out not to be UTF-8.
  NS_ENSURE_FALSE(aCharset.IsEmpty(), NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsICharsetConverterManager> charsetConverterManager;

  charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
  rv = charsetConverterManager->GetUnicodeDecoder(aCharset.get(), 
                                                  getter_AddRefs(unicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 srcLen = aURI.Length();
  PRInt32 dstLen;
  rv = unicodeDecoder->GetMaxLength(aURI.get(), srcLen, &dstLen);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar *ustr = (PRUnichar *) nsMemory::Alloc(dstLen * sizeof(PRUnichar));
  NS_ENSURE_TRUE(ustr, NS_ERROR_OUT_OF_MEMORY);

  rv = unicodeDecoder->Convert(aURI.get(), &srcLen, ustr, &dstLen);

  if (NS_SUCCEEDED(rv))
    _retval.Assign(ustr, dstLen);
  
  nsMemory::Free(ustr);

  return rv;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeURIForUI(const nsACString & aCharset, 
                                                const nsACString &aURIFragment, 
                                                nsAString &_retval)
{
  nsCAutoString unescapedSpec;
  // skip control octets (0x00 - 0x1f and 0x7f) when unescaping
  NS_UnescapeURL(PromiseFlatCString(aURIFragment), 
                 esc_SkipControl | esc_AlwaysCopy, unescapedSpec);

  return convertURItoUnicode(PromiseFlatCString(aCharset), unescapedSpec, PR_TRUE, _retval);
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeNonAsciiURI(const nsACString & aCharset, 
                                                   const nsACString &aURIFragment, 
                                                   nsAString &_retval)
{
  nsCAutoString unescapedSpec;
  NS_UnescapeURL(PromiseFlatCString(aURIFragment),
                 esc_AlwaysCopy | esc_OnlyNonASCII, unescapedSpec);

  return convertURItoUnicode(PromiseFlatCString(aCharset), unescapedSpec, PR_TRUE, _retval);
}

//----------------------------------------------------------------------
