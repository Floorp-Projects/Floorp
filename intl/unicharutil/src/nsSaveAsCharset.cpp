/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
nsSaveAsCharset::Init(const char *charset, PRUint32 attr, PRUint32 entityVersion)
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
  if (nsnull == _retval)
    return NS_ERROR_NULL_POINTER;
  if (nsnull == inString)
    return NS_ERROR_NULL_POINTER;
  if (0 == *inString)
    return NS_ERROR_ILLEGAL_VALUE;
  nsresult rv = NS_OK;

  NS_ASSERTION(mEncoder, "need to call Init() before Convert()");
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  *_retval = nsnull;

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
      PRUnichar *entity = nsnull;
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
    *aCharset = nsnull;
    NS_ASSERTION(charset, "make sure to call Init() with non empty charset list");
    return NS_ERROR_FAILURE;
  }

  *aCharset = nsCRT::strdup(charset);
  return (*aCharset) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/////////////////////////////////////////////////////////////////////////////////////////

// do the fallback, reallocate the buffer if necessary
// need to pass destination buffer info (size, current position and estimation of rest of the conversion)
NS_IMETHODIMP
nsSaveAsCharset::HandleFallBack(PRUint32 character, char **outString, PRInt32 *bufferLength, 
                                PRInt32 *currentPos, PRInt32 estimatedLength)
{
  if((nsnull == outString ) || (nsnull == bufferLength) ||(nsnull ==currentPos))
    return NS_ERROR_NULL_POINTER;
  char fallbackStr[256];
  nsresult rv = DoConversionFallBack(character, fallbackStr, 256);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 tempLen = (PRInt32) PL_strlen(fallbackStr);

    // reallocate if the buffer is not large enough
    if ((tempLen + estimatedLength) >= (*bufferLength - *currentPos)) {
      char *temp = (char *) PR_Realloc(*outString, *bufferLength + tempLen);
      if (NULL != temp) {
        // adjust length/pointer after realloc
        *bufferLength += tempLen;
        *outString = temp;
      } else {
        *outString = NULL;
        *bufferLength =0;
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
  if(nsnull == outString )
    return NS_ERROR_NULL_POINTER;
  NS_ASSERTION(outString, "invalid input");

  *outString = NULL;

  nsresult rv;
  PRInt32 inStringLength = nsCRT::strlen(inString);   // original input string length
  PRInt32 bufferLength;                               // allocated buffer length
  PRInt32 srcLength = inStringLength;
  PRInt32 dstLength;
  char *dstPtr = NULL;
  PRInt32 pos1, pos2;
  nsresult saveResult = NS_OK;                         // to remember NS_ERROR_UENC_NOMAPPING

  // estimate and allocate the target buffer (reserve extra memory for fallback)
  rv = mEncoder->GetMaxLength(inString, inStringLength, &dstLength);
  if (NS_FAILED(rv)) return rv;

  bufferLength = dstLength + 512; // reserve 512 byte for fallback.
  dstPtr = (char *) PR_Malloc(bufferLength);
  if (NULL == dstPtr) return NS_ERROR_OUT_OF_MEMORY;

  
  for (pos1 = 0, pos2 = 0; pos1 < inStringLength;) {
    // convert from unicode
    dstLength = bufferLength - pos2;
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
      PRUint32 unMappedChar;
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
nsSaveAsCharset::DoConversionFallBack(PRUint32 inUCS4, char *outString, PRInt32 bufferLength)
{
  NS_ASSERTION(outString, "invalid input");
  if(nsnull == outString )
    return NS_ERROR_NULL_POINTER;

  *outString = '\0';

  nsresult rv = NS_OK;

  if (ATTR_NO_FALLBACK(mAttribute)) {
    return NS_OK;
  }
  if (attr_EntityAfterCharsetConv == MASK_ENTITY(mAttribute)) {
    char *entity = NULL;
    rv = mEntityConverter->ConvertUTF32ToEntity(inUCS4, mEntityVersion, &entity);
    if (NS_SUCCEEDED(rv)) {
      if (NULL == entity || (PRInt32)strlen(entity) > bufferLength) {
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
  if ((mCharsetListIndex + 1) >= PRInt32(mCharsetList.Length()))
    return nsnull;

  // bump the index and return the next charset
  return mCharsetList[++mCharsetListIndex].get();
}
