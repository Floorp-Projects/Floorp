/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCollation.h"
#include "nsCollationCID.h"
#include "nsUnicharUtils.h"
#include "prmem.h"
#include "nsIUnicodeEncoder.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

////////////////////////////////////////////////////////////////////////////////

NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);

NS_IMPL_ISUPPORTS(nsCollationFactory, nsICollationFactory)

nsresult nsCollationFactory::CreateCollation(nsILocale* locale, nsICollation** instancePtr)
{
  // Create a collation interface instance.
  //
  nsICollation *inst;
  nsresult res;
  
  res = CallCreateInstance(kCollationCID, &inst);
  if (NS_FAILED(res)) {
    return res;
  }

  inst->Initialize(locale);
  *instancePtr = inst;

  return res;
}

////////////////////////////////////////////////////////////////////////////////

nsCollation::nsCollation()
{
  MOZ_COUNT_CTOR(nsCollation);
}

nsCollation::~nsCollation()
{
  MOZ_COUNT_DTOR(nsCollation);
}

nsresult nsCollation::NormalizeString(const nsAString& stringIn, nsAString& stringOut)
{
  int32_t aLength = stringIn.Length();

  if (aLength <= 64) {
    char16_t conversionBuffer[64];
    ToLowerCase(PromiseFlatString(stringIn).get(), conversionBuffer, aLength);
    stringOut.Assign(conversionBuffer, aLength);
  }
  else {
    char16_t* conversionBuffer;
    conversionBuffer = new char16_t[aLength];
    if (!conversionBuffer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    ToLowerCase(PromiseFlatString(stringIn).get(), conversionBuffer, aLength);
    stringOut.Assign(conversionBuffer, aLength);
    delete [] conversionBuffer;
  }
  return NS_OK;
}

nsresult nsCollation::SetCharset(const char* aCharset)
{
  NS_ENSURE_ARG_POINTER(aCharset);

  nsDependentCString label(aCharset);
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(label, encoding)) {
      return NS_ERROR_UCONV_NOCONV;
  }
  mEncoder = EncodingUtils::EncoderForEncoding(encoding);
  return NS_OK;
}

nsresult nsCollation::UnicodeToChar(const nsAString& aSrc, char** dst)
{
  NS_ENSURE_ARG_POINTER(dst);

  nsresult res = NS_OK;
  if (!mEncoder)
    res = SetCharset("ISO-8859-1");

  if (NS_SUCCEEDED(res)) {
    const nsPromiseFlatString& src = PromiseFlatString(aSrc);
    const char16_t *unichars = src.get();
    int32_t unicharLength = src.Length();
    int32_t dstLength;
    res = mEncoder->GetMaxLength(unichars, unicharLength, &dstLength);
    if (NS_SUCCEEDED(res)) {
      int32_t bufLength = dstLength + 1 + 32; // extra 32 bytes for Finish() call
      *dst = (char *) PR_Malloc(bufLength);
      if (*dst) {
        **dst = '\0';
        res = mEncoder->Convert(unichars, &unicharLength, *dst, &dstLength);

        if (NS_SUCCEEDED(res) || (NS_ERROR_UENC_NOMAPPING == res)) {
          // Finishes the conversion. The converter has the possibility to write some 
          // extra data and flush its final state.
          int32_t finishLength = bufLength - dstLength; // remaining unused buffer length
          if (finishLength > 0) {
            res = mEncoder->Finish((*dst + dstLength), &finishLength);
            if (NS_SUCCEEDED(res)) {
              (*dst)[dstLength + finishLength] = '\0';
            }
          }
        }
        if (NS_FAILED(res)) {
          PR_Free(*dst);
          *dst = nullptr;
        }
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return res;
}



