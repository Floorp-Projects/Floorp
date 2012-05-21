/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=4: 
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsString.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "prmem.h"
#include "nsUTF8ConverterService.h"
#include "nsEscape.h"
#include "nsAutoPtr.h"

NS_IMPL_ISUPPORTS1(nsUTF8ConverterService, nsIUTF8ConverterService)

static nsresult 
ToUTF8(const nsACString &aString, const char *aCharset, nsACString &aResult)
{
  nsresult rv;
  if (!aCharset || !*aCharset)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsICharsetConverterManager> ccm;

  ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
  rv = ccm->GetUnicodeDecoder(aCharset,
                              getter_AddRefs(unicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 srcLen = aString.Length();
  PRInt32 dstLen;
  const nsAFlatCString& inStr = PromiseFlatCString(aString);
  rv = unicodeDecoder->GetMaxLength(inStr.get(), srcLen, &dstLen);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoArrayPtr<PRUnichar> ustr(new PRUnichar[dstLen]);
  NS_ENSURE_TRUE(ustr, NS_ERROR_OUT_OF_MEMORY);

  rv = unicodeDecoder->Convert(inStr.get(), &srcLen, ustr, &dstLen);
  if (NS_SUCCEEDED(rv)){
    // Tru64 Cxx needs an explicit get()
    CopyUTF16toUTF8(Substring(ustr.get(), ustr + dstLen), aResult);
  }
  return rv;
}

NS_IMETHODIMP  
nsUTF8ConverterService::ConvertStringToUTF8(const nsACString &aString, 
                                            const char *aCharset, 
                                            bool aSkipCheck, 
                                            nsACString &aUTF8String)
{
  // return if ASCII only or valid UTF-8 providing that the ASCII/UTF-8
  // check is requested. It may not be asked for if a caller suspects
  // that the input is in non-ASCII 7bit charset (ISO-2022-xx, HZ) or 
  // it's in a charset other than UTF-8 that can be mistaken for UTF-8.
  if (!aSkipCheck && (IsASCII(aString) || IsUTF8(aString))) {
    aUTF8String = aString;
    return NS_OK;
  }

  aUTF8String.Truncate();

  nsresult rv = ToUTF8(aString, aCharset, aUTF8String);

  // additional protection for cases where check is skipped and  the input
  // is actually in UTF-8 as opposed to aCharset. (i.e. caller's hunch
  // was wrong.) We don't check ASCIIness assuming there's no charset
  // incompatible with ASCII (we don't support EBCDIC).
  if (aSkipCheck && NS_FAILED(rv) && IsUTF8(aString)) {
    aUTF8String = aString;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP  
nsUTF8ConverterService::ConvertURISpecToUTF8(const nsACString &aSpec, 
                                             const char *aCharset, 
                                             nsACString &aUTF8Spec)
{
  // assume UTF-8 if the spec contains unescaped non-ASCII characters.
  // No valid spec in Mozilla would break this assumption.
  if (!IsASCII(aSpec)) {
    aUTF8Spec = aSpec;
    return NS_OK;
  }

  aUTF8Spec.Truncate();

  nsCAutoString unescapedSpec; 
  // NS_UnescapeURL does not fill up unescapedSpec unless there's at least 
  // one character to unescape.
  bool written = NS_UnescapeURL(PromiseFlatCString(aSpec).get(), aSpec.Length(), 
                                  esc_OnlyNonASCII, unescapedSpec);

  if (!written) {
    aUTF8Spec = aSpec;
    return NS_OK;
  }
  // return if ASCII only or escaped UTF-8
  if (IsASCII(unescapedSpec) || IsUTF8(unescapedSpec)) {
    aUTF8Spec = unescapedSpec;
    return NS_OK;
  }

  return ToUTF8(unescapedSpec, aCharset, aUTF8Spec);
}

