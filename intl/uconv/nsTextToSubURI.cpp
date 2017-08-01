/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsString.h"
#include "nsITextToSubURI.h"
#include "nsEscape.h"
#include "nsTextToSubURI.h"
#include "nsCRT.h"
#include "mozilla/Encoding.h"
#include "mozilla/Preferences.h"
#include "nsISupportsPrimitives.h"

using namespace mozilla;

// Fallback value for the pref "network.IDN.blacklist_chars".
// UnEscapeURIForUI allows unescaped space; other than that, this is
// the same as the default "network.IDN.blacklist_chars" value.
static const char16_t sNetworkIDNBlacklistChars[] =
{
/*0x0020,*/
          0x00A0, 0x00BC, 0x00BD, 0x00BE, 0x01C3, 0x02D0, 0x0337,
  0x0338, 0x0589, 0x058A, 0x05C3, 0x05F4, 0x0609, 0x060A, 0x066A, 0x06D4,
  0x0701, 0x0702, 0x0703, 0x0704, 0x115F, 0x1160, 0x1735, 0x2000,
  0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008,
  0x2009, 0x200A, 0x200B, 0x200E, 0x200F, 0x2010, 0x2019, 0x2024, 0x2027, 0x2028,
  0x2029, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x202F, 0x2039,
  0x203A, 0x2041, 0x2044, 0x2052, 0x205F, 0x2153, 0x2154, 0x2155,
  0x2156, 0x2157, 0x2158, 0x2159, 0x215A, 0x215B, 0x215C, 0x215D,
  0x215E, 0x215F, 0x2215, 0x2236, 0x23AE, 0x2571, 0x29F6, 0x29F8,
  0x2AFB, 0x2AFD, 0x2FF0, 0x2FF1, 0x2FF2, 0x2FF3, 0x2FF4, 0x2FF5,
  0x2FF6, 0x2FF7, 0x2FF8, 0x2FF9, 0x2FFA, 0x2FFB, /*0x3000,*/ 0x3002,
  0x3014, 0x3015, 0x3033, 0x30A0, 0x3164, 0x321D, 0x321E, 0x33AE, 0x33AF,
  0x33C6, 0x33DF, 0xA789, 0xFE14, 0xFE15, 0xFE3F, 0xFE5D, 0xFE5E,
  0xFEFF, 0xFF0E, 0xFF0F, 0xFF61, 0xFFA0, 0xFFF9, 0xFFFA, 0xFFFB,
  0xFFFC, 0xFFFD
};

nsTextToSubURI::~nsTextToSubURI()
{
}

NS_IMPL_ISUPPORTS(nsTextToSubURI, nsITextToSubURI)

NS_IMETHODIMP
nsTextToSubURI::ConvertAndEscape(const nsACString& aCharset,
                                 const nsAString& aText,
                                 nsACString& aOut)
{
  auto encoding = Encoding::ForLabelNoReplacement(aCharset);
  if (!encoding) {
    aOut.Truncate();
    return NS_ERROR_UCONV_NOCONV;
  }
  nsresult rv;
  const Encoding* actualEncoding;
  nsAutoCString intermediate;
  Tie(rv, actualEncoding) = encoding->Encode(aText, intermediate);
  Unused << actualEncoding;
  if (NS_FAILED(rv)) {
    aOut.Truncate();
    return rv;
  }
  bool ok = NS_Escape(intermediate, aOut, url_XPAlphas);
  if (!ok) {
    aOut.Truncate();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextToSubURI::UnEscapeAndConvert(const nsACString& aCharset,
                                   const nsACString& aText,
                                   nsAString& aOut)
{
  auto encoding = Encoding::ForLabelNoReplacement(aCharset);
  if (!encoding) {
    aOut.Truncate();
    return NS_ERROR_UCONV_NOCONV;
  }
  nsAutoCString unescaped(aText);
  NS_UnescapeURL(unescaped);
  auto rv = encoding->DecodeWithoutBOMHandling(unescaped, aOut);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }
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

nsresult
nsTextToSubURI::convertURItoUnicode(const nsCString& aCharset,
                                    const nsCString& aURI,
                                    nsAString& aOut)
{
  // check for 7bit encoding the data may not be ASCII after we decode
  bool isStatefulCharset = statefulCharset(aCharset.get());

  if (!isStatefulCharset) {
    if (IsASCII(aURI)) {
      CopyASCIItoUTF16(aURI, aOut);
      return NS_OK;
    }
    if (IsUTF8(aURI)) {
      CopyUTF8toUTF16(aURI, aOut);
      return NS_OK;
    }
  }

  // empty charset could indicate UTF-8, but aURI turns out not to be UTF-8.
  NS_ENSURE_FALSE(aCharset.IsEmpty(), NS_ERROR_INVALID_ARG);

  auto encoding = Encoding::ForLabelNoReplacement(aCharset);
  if (!encoding) {
    aOut.Truncate();
    return NS_ERROR_UCONV_NOCONV;
  }
  return encoding->DecodeWithoutBOMHandlingAndWithoutReplacement(aURI, aOut);
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
    nsAdoptingString blacklist;
    nsresult rv = mozilla::Preferences::GetString("network.IDN.blacklist_chars",
                                                  blacklist);
    if (NS_SUCCEEDED(rv)) {
      // we allow SPACE and IDEOGRAPHIC SPACE in this method
      blacklist.StripChars(u" \u3000");
      mUnsafeChars.AppendElements(static_cast<const char16_t*>(blacklist.Data()),
                                  blacklist.Length());
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

NS_IMETHODIMP
nsTextToSubURI::UnEscapeNonAsciiURI(const nsACString& aCharset,
                                    const nsACString& aURIFragment,
                                    nsAString& _retval)
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

  nsresult rv = convertURItoUnicode(PromiseFlatCString(aCharset),
                                    unescapedSpec, _retval);
  // NS_OK_UDEC_MOREINPUT is a success code, so caller can't catch the error
  // if the string ends with a valid (but incomplete) sequence.
  return rv == NS_OK_UDEC_MOREINPUT ? NS_ERROR_UDEC_ILLEGALINPUT : rv;
}

//----------------------------------------------------------------------
