/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
#include "nsReadableUtils.h"
#include "nsITextToSubURI.h"
#include "nsIServiceManager.h"
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
  nsresult rv = NS_OK;
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager *ccm;
  rv = CallGetService(kCharsetConverterManagerCID, &ccm);
  if(NS_SUCCEEDED(rv)) {
     nsIUnicodeEncoder *encoder;
     rv = ccm->GetUnicodeEncoder(charset, &encoder);
     NS_RELEASE(ccm);
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
                pBuf = (char*)NS_Alloc(outlen+1);
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
             NS_Free(pBuf);
       }
       NS_RELEASE(encoder);
     }
  }
  
  return rv;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeAndConvert(
  const char *charset, const char *text, PRUnichar **_retval) 
{
  if(nsnull == _retval)
    return NS_ERROR_NULL_POINTER;
  if(nsnull == text) {
    // set empty string instead of returning error
    // due to compatibility for old version
    text = "";
  }
  *_retval = nsnull;
  nsresult rv = NS_OK;
  
  // unescape the string, unescape changes the input
  char *unescaped = NS_strdup(text);
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
        pBuf = (PRUnichar *) NS_Alloc((outlen+1)*sizeof(PRUnichar));
        if (nsnull == pBuf)
          rv = NS_ERROR_OUT_OF_MEMORY;
        else {
          if (NS_SUCCEEDED(rv = decoder->Convert(unescaped, &len, pBuf, &outlen))) {
            pBuf[outlen] = 0;
            *_retval = pBuf;
          }
          else
            NS_Free(pBuf);
        }
      }
      NS_RELEASE(decoder);
    }
  }
  NS_Free(unescaped);

  return rv;
}

static bool statefulCharset(const char *charset)
{
  if (!nsCRT::strncasecmp(charset, "ISO-2022-", sizeof("ISO-2022-")-1) ||
      !nsCRT::strcasecmp(charset, "UTF-7") ||
      !nsCRT::strcasecmp(charset, "HZ-GB-2312"))
    return true;

  return false;
}

nsresult nsTextToSubURI::convertURItoUnicode(const nsAFlatCString &aCharset,
                                             const nsAFlatCString &aURI, 
                                             bool aIRI, 
                                             nsAString &_retval)
{
  nsresult rv = NS_OK;

  // check for 7bit encoding the data may not be ASCII after we decode
  bool isStatefulCharset = statefulCharset(aCharset.get());

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

  PRUnichar *ustr = (PRUnichar *) NS_Alloc(dstLen * sizeof(PRUnichar));
  NS_ENSURE_TRUE(ustr, NS_ERROR_OUT_OF_MEMORY);

  rv = unicodeDecoder->Convert(aURI.get(), &srcLen, ustr, &dstLen);

  if (NS_SUCCEEDED(rv))
    _retval.Assign(ustr, dstLen);
  
  NS_Free(ustr);

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

  // in case of failure, return escaped URI
  // Test for != NS_OK rather than NS_FAILED, because incomplete multi-byte
  // sequences are also considered failure in this context
  if (convertURItoUnicode(
                PromiseFlatCString(aCharset), unescapedSpec, true, _retval)
      != NS_OK)
    // assume UTF-8 instead of ASCII  because hostname (IDN) may be in UTF-8
    CopyUTF8toUTF16(aURIFragment, _retval); 
  return NS_OK;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeNonAsciiURI(const nsACString & aCharset, 
                                                   const nsACString & aURIFragment, 
                                                   nsAString &_retval)
{
  nsCAutoString unescapedSpec;
  NS_UnescapeURL(PromiseFlatCString(aURIFragment),
                 esc_AlwaysCopy | esc_OnlyNonASCII, unescapedSpec);
  // leave the URI as it is if it's not UTF-8 and aCharset is not a ASCII
  // superset since converting "http:" with such an encoding is always a bad 
  // idea.
  if (!IsUTF8(unescapedSpec) && 
      (aCharset.LowerCaseEqualsLiteral("utf-16") ||
       aCharset.LowerCaseEqualsLiteral("utf-16be") ||
       aCharset.LowerCaseEqualsLiteral("utf-16le") ||
       aCharset.LowerCaseEqualsLiteral("utf-7") ||
       aCharset.LowerCaseEqualsLiteral("x-imap4-modified-utf7"))){
    CopyASCIItoUTF16(aURIFragment, _retval);
    return NS_OK;
  }

  return convertURItoUnicode(PromiseFlatCString(aCharset), unescapedSpec, true, _retval);
}

//----------------------------------------------------------------------
