/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "prmem.h"
#include "prprf.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsICharsetConverterManager.h"
#include "nsSaveAsCharset.h"
#include "nsCRT.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsWhitespaceTokenizer.h"

//
// nsISupports methods
//
NS_IMPL_ISUPPORTS1(nsSaveAsCharset, nsISaveAsCharset)

//
// nsSaveAsCharset
//
nsSaveAsCharset::nsSaveAsCharset()
{
  mAttribute = attr_htmlTextDefault;
  mEntityVersion = 0;
  mCharsetListIndex = -1;
}

nsSaveAsCharset::~nsSaveAsCharset()
{
}

NS_IMETHODIMP
nsSaveAsCharset::Init(const char *charset, uint32_t attr, uint32_t entityVersion)
{
  nsresult rv = NS_OK;

  mAttribute = attr;
  mEntityVersion = entityVersion;

  rv = SetupCharsetList(charset);
  NS_ENSURE_SUCCESS(rv, rv);

  // set up unicode encoder
  rv = SetupUnicodeEncoder(GetNextCharset());
  NS_ENSURE_SUCCESS(rv, rv);

  // set up entity converter
  if (attr_EntityNone != MASK_ENTITY(mAttribute) && !mEntityConverter)
    mEntityConverter = do_CreateInstance(NS_ENTITYCONVERTER_CONTRACTID, &rv);

  return rv;
}

NS_IMETHODIMP
nsSaveAsCharset::Convert(const PRUnichar *inString, char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG_POINTER(inString);
  if (0 == *inString)
    return NS_ERROR_ILLEGAL_VALUE;
  nsresult rv = NS_OK;

  NS_ASSERTION(mEncoder, "need to call Init() before Convert()");
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  *_retval = nullptr;

  // make sure to start from the first charset in the list
  if (mCharsetListIndex > 0) {
    mCharsetListIndex = -1;
    rv = SetupUnicodeEncoder(GetNextCharset());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  do {
    // fallback to the next charset in the list if the last conversion failed by an unmapped character
    if (MASK_CHARSET_FALLBACK(mAttribute) && NS_ERROR_UENC_NOMAPPING == rv) {
      const char * charset = GetNextCharset();
      if (!charset)
        break;
      rv = SetupUnicodeEncoder(charset);
      NS_ENSURE_SUCCESS(rv, rv);
      PR_FREEIF(*_retval);
    }

    if (attr_EntityBeforeCharsetConv == MASK_ENTITY(mAttribute)) {
      NS_ASSERTION(mEntityConverter, "need to call Init() before Convert()");
      NS_ENSURE_TRUE(mEntityConverter, NS_ERROR_FAILURE);
      PRUnichar *entity = nullptr;
      // do the entity conversion first
      rv = mEntityConverter->ConvertToEntities(inString, mEntityVersion, &entity);
      if(NS_SUCCEEDED(rv)) {
        rv = DoCharsetConversion(entity, _retval);
        nsMemory::Free(entity);
      }
    }
    else
      rv = DoCharsetConversion(inString, _retval);

  } while (MASK_CHARSET_FALLBACK(mAttribute) && NS_ERROR_UENC_NOMAPPING == rv);

  return rv;
}

NS_IMETHODIMP 
nsSaveAsCharset::GetCharset(char * *aCharset)
{
  NS_ENSURE_ARG(aCharset);
  NS_ASSERTION(mCharsetListIndex >= 0, "need to call Init() first");
  NS_ENSURE_TRUE(mCharsetListIndex >= 0, NS_ERROR_FAILURE);

  const char* charset = mCharsetList[mCharsetListIndex].get();
  if (!charset) {
    *aCharset = nullptr;
    NS_ASSERTION(charset, "make sure to call Init() with non empty charset list");
    return NS_ERROR_FAILURE;
  }

  *aCharset = strdup(charset);
  return (*aCharset) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/////////////////////////////////////////////////////////////////////////////////////////

#define RESERVE_FALLBACK_BYTES 512

// do the fallback, reallocate the buffer if necessary
// need to pass destination buffer info (size, current position and estimation of rest of the conversion)
NS_IMETHODIMP
nsSaveAsCharset::HandleFallBack(uint32_t character, char **outString, int32_t *bufferLength, 
                                int32_t *currentPos, int32_t estimatedLength)
{
  NS_ENSURE_ARG_POINTER(outString);
  NS_ENSURE_ARG_POINTER(bufferLength);
  NS_ENSURE_ARG_POINTER(currentPos);

  char fallbackStr[256];
  nsresult rv = DoConversionFallBack(character, fallbackStr, 256);
  if (NS_SUCCEEDED(rv)) {
    int32_t tempLen = (int32_t) strlen(fallbackStr);

    // reallocate if the buffer is not large enough
    if ((tempLen + estimatedLength) >= (*bufferLength - *currentPos)) {
      int32_t addLength = tempLen + RESERVE_FALLBACK_BYTES;
      // + 1 is for the terminating NUL, don't add that to bufferLength
      char *temp = (char *) PR_Realloc(*outString, *bufferLength + addLength + 1);
      if (temp) {
        // adjust length/pointer after realloc
        *bufferLength += addLength;
        *outString = temp;
      } else {
        *outString = nullptr;
        *bufferLength = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    memcpy((*outString + *currentPos), fallbackStr, tempLen);
    *currentPos += tempLen;
  }
  return rv;
}

NS_IMETHODIMP
nsSaveAsCharset::DoCharsetConversion(const PRUnichar *inString, char **outString)
{
  NS_ENSURE_ARG_POINTER(outString);

  *outString = nullptr;

  nsresult rv;
  int32_t inStringLength = NS_strlen(inString);       // original input string length
  int32_t bufferLength;                               // allocated buffer length
  int32_t srcLength = inStringLength;
  int32_t dstLength;
  int32_t pos1, pos2;
  nsresult saveResult = NS_OK;                         // to remember NS_ERROR_UENC_NOMAPPING

  // estimate and allocate the target buffer (reserve extra memory for fallback)
  rv = mEncoder->GetMaxLength(inString, inStringLength, &dstLength);
  if (NS_FAILED(rv)) return rv;

  bufferLength = dstLength + RESERVE_FALLBACK_BYTES; // extra bytes for fallback
  // + 1 is for the terminating NUL -- we don't add that to bufferLength so that
  // we can always write dstPtr[pos2] = '\0' even when the encoder filled the
  // buffer.
  char *dstPtr = (char *) PR_Malloc(bufferLength + 1);
  if (!dstPtr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  for (pos1 = 0, pos2 = 0; pos1 < inStringLength;) {
    // convert from unicode
    dstLength = bufferLength - pos2;
    NS_ASSERTION(dstLength >= 0, "out of bounds write");
    rv = mEncoder->Convert(&inString[pos1], &srcLength, &dstPtr[pos2], &dstLength);

    pos1 += srcLength ? srcLength : 1;
    pos2 += dstLength;
    dstPtr[pos2] = '\0';

    // break: this is usually the case (no error) OR unrecoverable error
    if (NS_ERROR_UENC_NOMAPPING != rv) break;

    // remember this happened and reset the result
    saveResult = rv;
    rv = NS_OK;

    // finish encoder, give it a chance to write extra data like escape sequences
    dstLength = bufferLength - pos2;
    rv = mEncoder->Finish(&dstPtr[pos2], &dstLength);
    if (NS_SUCCEEDED(rv)) {
      pos2 += dstLength;
      dstPtr[pos2] = '\0';
    }

    srcLength = inStringLength - pos1;

    // do the fallback
    if (!ATTR_NO_FALLBACK(mAttribute)) {
      uint32_t unMappedChar;
      if (NS_IS_HIGH_SURROGATE(inString[pos1-1]) && 
          inStringLength > pos1 && NS_IS_LOW_SURROGATE(inString[pos1])) {
        unMappedChar = SURROGATE_TO_UCS4(inString[pos1-1], inString[pos1]);
        pos1++;
      } else {
        unMappedChar = inString[pos1-1];
      }

      rv = mEncoder->GetMaxLength(inString+pos1, inStringLength-pos1, &dstLength);
      if (NS_FAILED(rv)) 
        break;

      rv = HandleFallBack(unMappedChar, &dstPtr, &bufferLength, &pos2, dstLength);
      if (NS_FAILED(rv)) 
        break;
      dstPtr[pos2] = '\0';
    }
  }

  if (NS_SUCCEEDED(rv)) {
    // finish encoder, give it a chance to write extra data like escape sequences
    dstLength = bufferLength - pos2;
    rv = mEncoder->Finish(&dstPtr[pos2], &dstLength);
    if (NS_SUCCEEDED(rv)) {
      pos2 += dstLength;
      dstPtr[pos2] = '\0';
    }
  }

  if (NS_FAILED(rv)) {
    PR_FREEIF(dstPtr);
    return rv;
  }

  *outString = dstPtr;      // set the result string

  // set error code so that the caller can do own fall back
  if (NS_ERROR_UENC_NOMAPPING == saveResult) {
    rv = NS_ERROR_UENC_NOMAPPING;
  }

  return rv;
}

NS_IMETHODIMP
nsSaveAsCharset::DoConversionFallBack(uint32_t inUCS4, char *outString, int32_t bufferLength)
{
  NS_ENSURE_ARG_POINTER(outString);

  *outString = '\0';

  nsresult rv = NS_OK;

  if (ATTR_NO_FALLBACK(mAttribute)) {
    return NS_OK;
  }
  if (attr_EntityAfterCharsetConv == MASK_ENTITY(mAttribute)) {
    char *entity = nullptr;
    rv = mEntityConverter->ConvertUTF32ToEntity(inUCS4, mEntityVersion, &entity);
    if (NS_SUCCEEDED(rv)) {
      if (!entity || (int32_t)strlen(entity) > bufferLength) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      PL_strcpy(outString, entity);
      nsMemory::Free(entity);
      return rv;
    }
  }

  switch (MASK_FALLBACK(mAttribute)) {
  case attr_FallbackQuestionMark:
    if(bufferLength>=2) {
      *outString++='?';
      *outString='\0';
      rv = NS_OK;
    } else {
      rv = NS_ERROR_FAILURE;
    }
    break;
  case attr_FallbackEscapeU:
    if (inUCS4 & 0xff0000)
      rv = (PR_snprintf(outString, bufferLength, "\\u%.6x", inUCS4) > 0) ? NS_OK : NS_ERROR_FAILURE;
    else
      rv = (PR_snprintf(outString, bufferLength, "\\u%.4x", inUCS4) > 0) ? NS_OK : NS_ERROR_FAILURE;
    break;
  case attr_FallbackDecimalNCR:
    rv = ( PR_snprintf(outString, bufferLength, "&#%u;", inUCS4) > 0) ? NS_OK : NS_ERROR_FAILURE;
    break;
  case attr_FallbackHexNCR:
    rv = (PR_snprintf(outString, bufferLength, "&#x%x;", inUCS4) > 0) ? NS_OK : NS_ERROR_FAILURE;
    break;
  case attr_FallbackNone:
    rv = NS_OK;
    break;
  default:
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  }

	return rv;
}

nsresult nsSaveAsCharset::SetupUnicodeEncoder(const char* charset)
{
  NS_ENSURE_ARG(charset);
  nsresult rv;

  // set up unicode encoder
  nsCOMPtr <nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return ccm->GetUnicodeEncoder(charset, getter_AddRefs(mEncoder));
}

nsresult nsSaveAsCharset::SetupCharsetList(const char *charsetList)
{
  NS_ENSURE_ARG(charsetList);

  NS_ASSERTION(charsetList[0], "charsetList should not be empty");
  if (!charsetList[0])
    return NS_ERROR_INVALID_ARG;

  if (mCharsetListIndex >= 0) {
    mCharsetList.Clear();
    mCharsetListIndex = -1;
  }

  nsCWhitespaceTokenizer tokenizer = nsDependentCString(charsetList);
  while (tokenizer.hasMoreTokens()) {
    ParseString(tokenizer.nextToken(), ',', mCharsetList);
  }

  return NS_OK;
}

const char * nsSaveAsCharset::GetNextCharset()
{
  if ((mCharsetListIndex + 1) >= int32_t(mCharsetList.Length()))
    return nullptr;

  // bump the index and return the next charset
  return mCharsetList[++mCharsetListIndex].get();
}
