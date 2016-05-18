/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsITextToSubURI.h"
#include "nsEscape.h"
#include "nsTextToSubURI.h"
#include "nsCRT.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/Preferences.h"
#include "nsISupportsPrimitives.h"

using mozilla::dom::EncodingUtils;

// Fallback value for the pref "network.IDN.blacklist_chars".
// UnEscapeURIForUI allows unescaped space; other than that, this is
// the same as the default "network.IDN.blacklist_chars" value.
static const char16_t sNetworkIDNBlacklistChars[] =
{
/*0x0020,*/
          0x00A0, 0x00BC, 0x00BD, 0x00BE, 0x01C3, 0x02D0, 0x0337,
  0x0338, 0x0589, 0x05C3, 0x05F4, 0x0609, 0x060A, 0x066A, 0x06D4,
  0x0701, 0x0702, 0x0703, 0x0704, 0x115F, 0x1160, 0x1735, 0x2000,
  0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008,
  0x2009, 0x200A, 0x200B, 0x200E, 0x200F, 0x2024, 0x2027, 0x2028,
  0x2029, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x202F, 0x2039,
  0x203A, 0x2041, 0x2044, 0x2052, 0x205F, 0x2153, 0x2154, 0x2155,
  0x2156, 0x2157, 0x2158, 0x2159, 0x215A, 0x215B, 0x215C, 0x215D,
  0x215E, 0x215F, 0x2215, 0x2236, 0x23AE, 0x2571, 0x29F6, 0x29F8,
  0x2AFB, 0x2AFD, 0x2FF0, 0x2FF1, 0x2FF2, 0x2FF3, 0x2FF4, 0x2FF5,
  0x2FF6, 0x2FF7, 0x2FF8, 0x2FF9, 0x2FFA, 0x2FFB, 0x3000, 0x3002,
  0x3014, 0x3015, 0x3033, 0x3164, 0x321D, 0x321E, 0x33AE, 0x33AF,
  0x33C6, 0x33DF, 0xA789, 0xFE14, 0xFE15, 0xFE3F, 0xFE5D, 0xFE5E,
  0xFEFF, 0xFF0E, 0xFF0F, 0xFF61, 0xFFA0, 0xFFF9, 0xFFFA, 0xFFFB,
  0xFFFC, 0xFFFD
};

nsTextToSubURI::~nsTextToSubURI()
{
}

NS_IMPL_ISUPPORTS(nsTextToSubURI, nsITextToSubURI)

NS_IMETHODIMP  nsTextToSubURI::ConvertAndEscape(
  const char *charset, const char16_t *text, char **_retval) 
{
  if (!_retval) {
    return NS_ERROR_NULL_POINTER;
  }
  *_retval = nullptr;
  nsresult rv = NS_OK;
  
  if (!charset) {
    return NS_ERROR_NULL_POINTER;
  }

  nsDependentCString label(charset);
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(label, encoding)) {
    return NS_ERROR_UCONV_NOCONV;
  }
  nsCOMPtr<nsIUnicodeEncoder> encoder =
    EncodingUtils::EncoderForEncoding(encoding);
  rv = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nullptr, (char16_t)'?');
  if (NS_SUCCEEDED(rv) ) {
    char buf[256];
    char *pBuf = buf;
    int32_t ulen = text ? NS_strlen(text) : 0;
    int32_t outlen = 0;
    if (NS_SUCCEEDED(rv = encoder->GetMaxLength(text, ulen, &outlen))) {
      if (outlen >= 256) {
        pBuf = (char*)moz_xmalloc(outlen+1);
      }
      if (nullptr == pBuf) {
        outlen = 255;
        pBuf = buf;
      }
      int32_t bufLen = outlen;
      if (NS_SUCCEEDED(rv = encoder->Convert(text,&ulen, pBuf, &outlen))) {
        // put termination characters (e.g. ESC(B of ISO-2022-JP) if necessary
        int32_t finLen = bufLen - outlen;
        if (finLen > 0) {
          if (NS_SUCCEEDED(encoder->Finish((char *)(pBuf+outlen), &finLen))) {
            outlen += finLen;
          }
        }
        *_retval = nsEscape(pBuf, outlen, nullptr, url_XPAlphas);
        if (nullptr == *_retval) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    if (pBuf != buf) {
      free(pBuf);
    }
  }
  
  return rv;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeAndConvert(
  const char *charset, const char *text, char16_t **_retval) 
{
  if(nullptr == _retval)
    return NS_ERROR_NULL_POINTER;
  if(nullptr == text) {
    // set empty string instead of returning error
    // due to compatibility for old version
    text = "";
  }
  *_retval = nullptr;
  nsresult rv = NS_OK;
  
  if (!charset) {
    return NS_ERROR_NULL_POINTER;
  }


  // unescape the string, unescape changes the input
  char *unescaped = NS_strdup(text);
  if (nullptr == unescaped)
    return NS_ERROR_OUT_OF_MEMORY;
  unescaped = nsUnescape(unescaped);
  NS_ASSERTION(unescaped, "nsUnescape returned null");

  nsDependentCString label(charset);
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(label, encoding)) {
    return NS_ERROR_UCONV_NOCONV;
  }
  nsCOMPtr<nsIUnicodeDecoder> decoder =
    EncodingUtils::DecoderForEncoding(encoding);
  char16_t *pBuf = nullptr;
  int32_t len = strlen(unescaped);
  int32_t outlen = 0;
  if (NS_SUCCEEDED(rv = decoder->GetMaxLength(unescaped, len, &outlen))) {
    pBuf = (char16_t *) moz_xmalloc((outlen+1)*sizeof(char16_t));
    if (nullptr == pBuf) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
      if (NS_SUCCEEDED(rv = decoder->Convert(unescaped, &len, pBuf, &outlen))) {
        pBuf[outlen] = 0;
        *_retval = pBuf;
      } else {
        free(pBuf);
      }
    }
  }
  free(unescaped);

  return rv;
}

static bool statefulCharset(const char *charset)
{
  // HZ, UTF-7 and the CN and KR ISO-2022 variants are no longer in
  // mozilla-central but keeping them here just in case for the benefit of
  // comm-central.
  if (!nsCRT::strncasecmp(charset, "ISO-2022-", sizeof("ISO-2022-")-1) ||
      !nsCRT::strcasecmp(charset, "UTF-7") ||
      !nsCRT::strcasecmp(charset, "HZ-GB-2312"))
    return true;

  return false;
}

nsresult nsTextToSubURI::convertURItoUnicode(const nsAFlatCString &aCharset,
                                             const nsAFlatCString &aURI, 
                                             nsAString &_retval)
{
  // check for 7bit encoding the data may not be ASCII after we decode
  bool isStatefulCharset = statefulCharset(aCharset.get());

  if (!isStatefulCharset) {
    if (IsASCII(aURI)) {
      CopyASCIItoUTF16(aURI, _retval);
      return NS_OK;
    }
    if (IsUTF8(aURI)) {
      CopyUTF8toUTF16(aURI, _retval);
      return NS_OK;
    }
  }

  // empty charset could indicate UTF-8, but aURI turns out not to be UTF-8.
  NS_ENSURE_FALSE(aCharset.IsEmpty(), NS_ERROR_INVALID_ARG);

  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(aCharset, encoding)) {
    return NS_ERROR_UCONV_NOCONV;
  }
  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder =
    EncodingUtils::DecoderForEncoding(encoding);

  unicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Signal);

  int32_t srcLen = aURI.Length();
  int32_t dstLen;
  nsresult rv = unicodeDecoder->GetMaxLength(aURI.get(), srcLen, &dstLen);
  NS_ENSURE_SUCCESS(rv, rv);

  char16_t *ustr = (char16_t *) moz_xmalloc(dstLen * sizeof(char16_t));
  NS_ENSURE_TRUE(ustr, NS_ERROR_OUT_OF_MEMORY);

  rv = unicodeDecoder->Convert(aURI.get(), &srcLen, ustr, &dstLen);

  if (NS_SUCCEEDED(rv))
    _retval.Assign(ustr, dstLen);
  
  free(ustr);

  return rv;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeURIForUI(const nsACString & aCharset, 
                                                const nsACString &aURIFragment, 
                                                nsAString &_retval)
{
  nsAutoCString unescapedSpec;
  // skip control octets (0x00 - 0x1f and 0x7f) when unescaping
  NS_UnescapeURL(PromiseFlatCString(aURIFragment), 
                 esc_SkipControl | esc_AlwaysCopy, unescapedSpec);

  // in case of failure, return escaped URI
  // Test for != NS_OK rather than NS_FAILED, because incomplete multi-byte
  // sequences are also considered failure in this context
  if (convertURItoUnicode(
                PromiseFlatCString(aCharset), unescapedSpec, _retval)
      != NS_OK) {
    // assume UTF-8 instead of ASCII  because hostname (IDN) may be in UTF-8
    CopyUTF8toUTF16(aURIFragment, _retval);
  }

  // If there are any characters that are unsafe for URIs, reescape those.
  if (mUnsafeChars.IsEmpty()) {
    nsCOMPtr<nsISupportsString> blacklist;
    nsresult rv = mozilla::Preferences::GetComplex("network.IDN.blacklist_chars",
                                                   NS_GET_IID(nsISupportsString),
                                                   getter_AddRefs(blacklist));
    if (NS_SUCCEEDED(rv)) {
      nsString chars;
      blacklist->ToString(getter_Copies(chars));
      chars.StripChars(" "); // we allow SPACE in this method
      mUnsafeChars.AppendElements(static_cast<const char16_t*>(chars.Data()), chars.Length());
    } else {
      NS_WARNING("Failed to get the 'network.IDN.blacklist_chars' preference");
    }
    // We check IsEmpty() intentionally here because an empty (or just spaces)
    // pref value is likely a mistake/error of some sort.
    if (mUnsafeChars.IsEmpty()) {
      mUnsafeChars.AppendElements(sNetworkIDNBlacklistChars,
                                  mozilla::ArrayLength(sNetworkIDNBlacklistChars));
    }
    mUnsafeChars.Sort();
  }
  const nsPromiseFlatString& unescapedResult = PromiseFlatString(_retval);
  nsString reescapedSpec;
  _retval = NS_EscapeURL(unescapedResult, mUnsafeChars, reescapedSpec);

  return NS_OK;
}

NS_IMETHODIMP  nsTextToSubURI::UnEscapeNonAsciiURI(const nsACString & aCharset, 
                                                   const nsACString & aURIFragment, 
                                                   nsAString &_retval)
{
  nsAutoCString unescapedSpec;
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

  return convertURItoUnicode(PromiseFlatCString(aCharset), unescapedSpec, _retval);
}

//----------------------------------------------------------------------
