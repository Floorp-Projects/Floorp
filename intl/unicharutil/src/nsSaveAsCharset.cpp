/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "prmem.h"
#include "prprf.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsSaveAsCharset.h"

//
// guids
//
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);


//
// nsISupports methods
//
NS_IMPL_ISUPPORTS1(nsSaveAsCharset, nsISaveAsCharset)


//
// nsSaveAsCharset
//
nsSaveAsCharset::nsSaveAsCharset()
{
  NS_INIT_REFCNT();

  mAttribute = attr_htmlTextDefault;
  mEntityVersion = 0;
  mEncoder = NULL;
  mEntityConverter = NULL;
}

nsSaveAsCharset::~nsSaveAsCharset()
{
  NS_IF_RELEASE(mEncoder);
  NS_IF_RELEASE(mEntityConverter);
}

NS_IMETHODIMP
nsSaveAsCharset::Init(const char *charset, PRUint32 attr, PRUint32 entityVersion)
{
  nsresult rv = NS_OK;

  nsString aCharset; aCharset.AssignWithConversion(charset);
  mAttribute = attr;
  mEntityVersion = entityVersion;

  // set up unicode encoder
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(kCharsetConverterManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = ccm->GetUnicodeEncoder(&aCharset, &mEncoder);
  if (NS_FAILED(rv)) return rv;
  if (NULL == mEncoder) return NS_ERROR_FAILURE;

  // set up entity converter
  if (attr_EntityNone != MASK_ENTITY(mAttribute)) {
    rv = nsComponentManager::CreateInstance(kEntityConverterCID, 
                                            NULL,
                                            NS_GET_IID(nsIEntityConverter), 
                                            (void**)&mEntityConverter);
    if (NULL == mEntityConverter) return NS_ERROR_FAILURE;
  }

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
  nsresult rv;

  if (NULL == mEncoder) return NS_ERROR_FAILURE;  // need to call Init() before Convert()

  if (attr_EntityBeforeCharsetConv == MASK_ENTITY(mAttribute)) {
    if (NULL == mEntityConverter) return NS_ERROR_FAILURE;
    PRUnichar *entity = NULL;
    // do the entity conversion first
    rv = mEntityConverter->ConvertToEntities(inString, mEntityVersion, &entity);
    if(NS_SUCCEEDED(rv)) {
      if (NULL == entity) return NS_ERROR_OUT_OF_MEMORY;
      rv = DoCharsetConversion(entity, _retval);
      nsMemory::Free(entity);
    }
  }
  else {
    rv = DoCharsetConversion(inString, _retval);
  }

  return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////

// do the fallback, reallocate the buffer if necessary
// need to pass destination buffer info (size, current position and estimation of rest of the conversion)
NS_IMETHODIMP
nsSaveAsCharset::HandleFallBack(PRUnichar character, char **outString, PRInt32 *bufferLength, 
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
    nsCRT::memcpy((*outString + *currentPos), fallbackStr, tempLen);
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
      PRUnichar unMappedChar = inString[pos1-1];

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
nsSaveAsCharset::DoConversionFallBack(PRUnichar inCharacter, char *outString, PRInt32 bufferLength)
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
    rv = mEntityConverter->ConvertToEntity(inCharacter, mEntityVersion, &entity);
    if (NS_SUCCEEDED(rv)) {
      if (NULL == entity || (PRInt32)nsCRT::strlen(entity) > bufferLength) {
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
    rv = (PR_snprintf(outString, bufferLength, "\\u%.4x", inCharacter) > 0) ? NS_OK : NS_ERROR_FAILURE;
    break;
  case attr_FallbackDecimalNCR:
    rv = ( PR_snprintf(outString, bufferLength, "&#%u;", inCharacter) > 0) ? NS_OK : NS_ERROR_FAILURE;
    break;
  case attr_FallbackHexNCR:
    rv = (PR_snprintf(outString, bufferLength, "&#x%x;", inCharacter) > 0) ? NS_OK : NS_ERROR_FAILURE;
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


/////////////////////////////////////////////////////////////////////////////////////////

nsresult 
NS_NewSaveAsCharset(nsISupports **inst)
{
  if(nsnull == inst )
    return NS_ERROR_NULL_POINTER;
  *inst = (nsISupports *) new nsSaveAsCharset;
   if(*inst)
      NS_ADDREF(*inst);
   return (*inst) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
