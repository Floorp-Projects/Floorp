/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsIScriptableUConv.h"
#include "nsScriptableUConv.h"
#include "nsIStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

/* Implementation file */
NS_IMPL_ISUPPORTS(nsScriptableUnicodeConverter, nsIScriptableUnicodeConverter)

nsScriptableUnicodeConverter::nsScriptableUnicodeConverter()
: mIsInternal(false)
{
}

nsScriptableUnicodeConverter::~nsScriptableUnicodeConverter()
{
}

nsresult
nsScriptableUnicodeConverter::ConvertFromUnicodeWithLength(const nsAString& aSrc,
                                                           int32_t* aOutLen,
                                                           char **_retval)
{
  if (!mEncoder)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  int32_t inLength = aSrc.Length();
  const nsAFlatString& flatSrc = PromiseFlatString(aSrc);
  rv = mEncoder->GetMaxLength(flatSrc.get(), inLength, aOutLen);
  if (NS_SUCCEEDED(rv)) {
    *_retval = (char*)moz_malloc(*aOutLen+1);
    if (!*_retval)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = mEncoder->Convert(flatSrc.get(), &inLength, *_retval, aOutLen);
    if (NS_SUCCEEDED(rv))
    {
      (*_retval)[*aOutLen] = '\0';
      return NS_OK;
    }
    moz_free(*_retval);
  }
  *_retval = nullptr;
  return NS_ERROR_FAILURE;
}

/* ACString ConvertFromUnicode (in AString src); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertFromUnicode(const nsAString& aSrc,
                                                 nsACString& _retval)
{
  int32_t len;
  char* str;
  nsresult rv = ConvertFromUnicodeWithLength(aSrc, &len, &str);
  if (NS_SUCCEEDED(rv)) {
    // No Adopt on nsACString :(
    if (!_retval.Assign(str, len, mozilla::fallible_t())) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    moz_free(str);
  }
  return rv;
}

nsresult
nsScriptableUnicodeConverter::FinishWithLength(char **_retval, int32_t* aLength)
{
  if (!mEncoder)
    return NS_ERROR_FAILURE;

  int32_t finLength = 32;

  *_retval = (char *)moz_malloc(finLength);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mEncoder->Finish(*_retval, &finLength);
  if (NS_SUCCEEDED(rv))
    *aLength = finLength;
  else
    moz_free(*_retval);

  return rv;

}

/* ACString Finish(); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::Finish(nsACString& _retval)
{
  int32_t len;
  char* str;
  nsresult rv = FinishWithLength(&str, &len);
  if (NS_SUCCEEDED(rv)) {
    // No Adopt on nsACString :(
    if (!_retval.Assign(str, len, mozilla::fallible_t())) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    moz_free(str);
  }
  return rv;
}

/* AString ConvertToUnicode (in ACString src); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToUnicode(const nsACString& aSrc, nsAString& _retval)
{
  nsACString::const_iterator i;
  aSrc.BeginReading(i);
  return ConvertFromByteArray(reinterpret_cast<const uint8_t*>(i.get()),
                              aSrc.Length(),
                              _retval);
}

/* AString convertFromByteArray([const,array,size_is(aCount)] in octet aData,
                                in unsigned long aCount);
 */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertFromByteArray(const uint8_t* aData,
                                                   uint32_t aCount,
                                                   nsAString& _retval)
{
  if (!mDecoder)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  int32_t inLength = aCount;
  int32_t outLength;
  rv = mDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                              inLength, &outLength);
  if (NS_SUCCEEDED(rv))
  {
    char16_t* buf = (char16_t*)moz_malloc((outLength+1)*sizeof(char16_t));
    if (!buf)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = mDecoder->Convert(reinterpret_cast<const char*>(aData),
                           &inLength, buf, &outLength);
    if (NS_SUCCEEDED(rv))
    {
      buf[outLength] = 0;
      if (!_retval.Assign(buf, outLength, mozilla::fallible_t())) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
    moz_free(buf);
    return rv;
  }
  return NS_ERROR_FAILURE;

}

/* void convertToByteArray(in AString aString,
                          [optional] out unsigned long aLen,
                          [array, size_is(aLen),retval] out octet aData);
 */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToByteArray(const nsAString& aString,
                                                 uint32_t* aLen,
                                                 uint8_t** _aData)
{
  char* data;
  int32_t len;
  nsresult rv = ConvertFromUnicodeWithLength(aString, &len, &data);
  if (NS_FAILED(rv))
    return rv;
  nsXPIDLCString str;
  str.Adopt(data, len); // NOTE: This uses the XPIDLCString as a byte array

  rv = FinishWithLength(&data, &len);
  if (NS_FAILED(rv))
    return rv;

  str.Append(data, len);
  moz_free(data);
  // NOTE: this being a byte array, it needs no null termination
  *_aData = reinterpret_cast<uint8_t*>(moz_malloc(str.Length()));
  if (!*_aData)
    return NS_ERROR_OUT_OF_MEMORY;
  memcpy(*_aData, str.get(), str.Length());
  *aLen = str.Length();
  return NS_OK;
}

/* nsIInputStream convertToInputStream(in AString aString); */
NS_IMETHODIMP
nsScriptableUnicodeConverter::ConvertToInputStream(const nsAString& aString,
                                                   nsIInputStream** _retval)
{
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> inputStream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  uint8_t* data;
  uint32_t dataLen;
  rv = ConvertToByteArray(aString, &dataLen, &data);
  if (NS_FAILED(rv))
    return rv;

  rv = inputStream->AdoptData(reinterpret_cast<char*>(data), dataLen);
  if (NS_FAILED(rv)) {
    moz_free(data);
    return rv;
  }

  NS_ADDREF(*_retval = inputStream);
  return rv;
}

/* attribute string charset; */
NS_IMETHODIMP
nsScriptableUnicodeConverter::GetCharset(char * *aCharset)
{
  *aCharset = ToNewCString(mCharset);
  if (!*aCharset)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::SetCharset(const char * aCharset)
{
  mCharset.Assign(aCharset);
  return InitConverter();
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::GetIsInternal(bool *aIsInternal)
{
  *aIsInternal = mIsInternal;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableUnicodeConverter::SetIsInternal(const bool aIsInternal)
{
  mIsInternal = aIsInternal;
  return NS_OK;
}

nsresult
nsScriptableUnicodeConverter::InitConverter()
{
  mEncoder = nullptr;
  mDecoder = nullptr;

  nsAutoCString encoding;
  if (mIsInternal) {
    // For compatibility with legacy extensions, let's try to see if the label
    // happens to be ASCII-case-insensitively an encoding. This should allow
    // for things like "utf-7" and "x-Mac-Hebrew".
    nsAutoCString contractId;
    nsAutoCString label(mCharset);
    EncodingUtils::TrimSpaceCharacters(label);
    // Let's try in lower case if we didn't get an decoder. E.g. x-mac-ce
    // and x-imap4-modified-utf7 are all lower case.
    ToLowerCase(label);
    if (label.EqualsLiteral("replacement")) {
      // reject "replacement"
      return NS_ERROR_UCONV_NOCONV;
    }
    contractId.AssignLiteral(NS_UNICODEENCODER_CONTRACTID_BASE);
    contractId.Append(label);
    mEncoder = do_CreateInstance(contractId.get());
    contractId.AssignLiteral(NS_UNICODEDECODER_CONTRACTID_BASE);
    contractId.Append(label);
    mDecoder = do_CreateInstance(contractId.get());
    if (!mDecoder) {
      // The old code seemed to want both a decoder and an encoder. Since some
      // internal encodings will be decoder-only in the future, let's relax
      // this. Note that the other methods check mEncoder for null anyway.
      // Let's try the upper case. E.g. UTF-7 and ISO-2022-CN have upper
      // case Gecko-canonical names.
      ToUpperCase(label);
      contractId.AssignLiteral(NS_UNICODEENCODER_CONTRACTID_BASE);
      contractId.Append(label);
      mEncoder = do_CreateInstance(contractId.get());
      contractId.AssignLiteral(NS_UNICODEDECODER_CONTRACTID_BASE);
      contractId.Append(label);
      mDecoder = do_CreateInstance(contractId.get());
      // If still no decoder, use the normal non-internal case below.
    }
  }

  if (!mDecoder) {
    if (!EncodingUtils::FindEncodingForLabelNoReplacement(mCharset, encoding)) {
      return NS_ERROR_UCONV_NOCONV;
    }
    mEncoder = EncodingUtils::EncoderForEncoding(encoding);
    mDecoder = EncodingUtils::DecoderForEncoding(encoding);
  }

  // The UTF-8 decoder used to throw regardless of the error behavior.
  // Simulating the old behavior for compatibility with legacy callers
  // (including addons). If callers want a control over the behavior,
  // they should switch to TextDecoder.
  if (encoding.EqualsLiteral("UTF-8")) {
    mDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Signal);
  }

  if (!mEncoder) {
    return NS_OK;
  }

  return mEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace,
                                          nullptr,
                                          (char16_t)'?');
}
