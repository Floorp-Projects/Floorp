/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */ 

// as does this
#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsIServiceManager.h"

#include "nsISupports.h"
#include "nsIPref.h"
#include "nsIMimeConverter.h"
#include "msgCore.h"
#include "rosetta_mailnews.h"
#include "nsMsgI18N.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsMsgMimeCID.h"
#include "nsMimeTypes.h"
#include "nsIEntityConverter.h"
#include "nsISaveAsCharset.h"
#include "nsHankakuToZenkakuCID.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

//
// International functions necessary for composition
//

// Convert an unicode string to a C string with a given charset.
nsresult ConvertFromUnicode(const nsString& aCharset, 
                            const nsString& inString,
                            char** outCString)
{
  *outCString = NULL;

  if (inString.IsEmpty()) {
    *outCString = nsCRT::strdup("");
    return (NULL == *outCString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }
  // Note: this will hide a possible error when the unicode text may contain more than one charset.
  // (e.g. Latin1 + Japanese). Use nsMsgI18NSaveAsCharset instead to avoid that problem.
  else if (aCharset.IsEmpty() ||
      aCharset.EqualsIgnoreCase("us-ascii") ||
      aCharset.EqualsIgnoreCase("ISO-8859-1")) {
    *outCString = inString.ToNewCString();
    return (NULL == *outCString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }
  else if (aCharset.EqualsIgnoreCase("UTF-8")) {
    *outCString = inString.ToNewUTF8String();
    return (NULL == *outCString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }

  nsAutoString convCharset;
  nsresult res;

  // Resolve charset alias
  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }
  if (NS_FAILED(res)) {
    return res;
  }

  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res)) {
    nsIUnicodeEncoder* encoder = nsnull;

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&convCharset, &encoder);
    if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
      PRUnichar *unichars = (PRUnichar *) inString.GetUnicode();
      PRInt32 unicharLength = inString.Length();
      PRInt32 dstLength;
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      // allocale an output buffer
      *outCString = (char *) PR_Malloc(dstLength + 1);
      if (nsnull != *outCString) {
        PRInt32 oldUnicharLength = unicharLength;
        char *tempCString = *outCString;
        PRInt32 totalCLength = 0;
        while (1) {
          res = encoder->Convert(unichars, &unicharLength, tempCString, &dstLength);

          // increment for destination
          tempCString += dstLength;
          totalCLength += dstLength;

          // break: this is usually the case
          // source length <= zero and no error or unrecoverable error
          if (0 >= unicharLength || NS_ERROR_UENC_NOMAPPING != res) {
            break;
          }
          // could not map unicode to the destination charset
          // increment for source unicode and continue
          unichars += unicharLength;
          oldUnicharLength -= unicharLength;   // adjust availabe buffer size
          unicharLength = oldUnicharLength;
          // reset the encoder
          encoder->Reset();
        }
        (*outCString)[totalCLength] = '\0';
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      NS_IF_RELEASE(encoder);
    }    
  }
  return res;
}

// Convert a C string to an unicode string.
nsresult ConvertToUnicode(const nsString& aCharset, 
                          const char* inCString, 
                          nsString& outString)
{
  if (NULL == inCString) {
    return NS_ERROR_NULL_POINTER;
  }
  else if ('\0' == *inCString) {
    outString.Truncate();
    return NS_OK;
  }
  else if (aCharset.IsEmpty() ||
      aCharset.EqualsIgnoreCase("us-ascii") ||
      aCharset.EqualsIgnoreCase("ISO-8859-1")) {
    outString.Assign(inCString);
    return NS_OK;
  }

  nsAutoString convCharset;
  nsresult res;

  // Resolve charset alias
  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }
  if (NS_FAILED(res)) {
    return res;
  }

  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeDecoder* decoder = nsnull;
    PRUnichar *unichars;
    PRInt32 unicharLength;

    // get an unicode converter
    res = ccm->GetUnicodeDecoder(&convCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) {
      PRInt32 srcLen = PL_strlen(inCString);
      res = decoder->GetMaxLength(inCString, srcLen, &unicharLength);
      // allocale an output buffer
      unichars = (PRUnichar *) PR_Malloc(unicharLength * sizeof(PRUnichar));
      if (unichars != nsnull) {
        // convert to unicode
        res = decoder->Convert(inCString, &srcLen, unichars, &unicharLength);
        outString.Assign(unichars, unicharLength);
        PR_Free(unichars);
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      NS_IF_RELEASE(decoder);
    }    
  }  
  return res;
}

// Charset to be used for the internatl processing.
const char *msgCompHeaderInternalCharset()
{
  // UTF-8 is a super set of us-ascii. 
  // We can use the same string manipulation methods as us-ascii without breaking non us-ascii characters. 
  return "UTF-8";
}

// Charset used by the file system.
const nsString& msgCompFileSystemCharset()
{
	/* Get a charset used for the file. */
	static nsString aPlatformCharset;

	if (aPlatformCharset.Length() < 1) 
	{
		nsresult rv;
		nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_PROGID, &rv);
		if (NS_SUCCEEDED(rv)) 
			rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aPlatformCharset);

		if (NS_FAILED(rv)) 
			aPlatformCharset.Assign("ISO-8859-1");
	}
	return aPlatformCharset;
}

// MIME encoder, output string should be freed by PR_FREE
char * nsMsgI18NEncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime) 
{
  // No MIME, just duplicate the string.
  if (PR_FALSE == bUseMime) {
    return PL_strdup(header);
  }

  char *encodedString = nsnull;
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           NS_GET_IID(nsIMimeConverter), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->EncodeMimePartIIStr_UTF8(header, charset, kMIME_ENCODED_WORD_SIZE, &encodedString);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? encodedString : nsnull;
}

// MIME decoder
nsresult nsMsgI18NDecodeMimePartIIStr(const nsString& header, nsString& charset, nsString& decodedString, PRBool eatContinuations)
{
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                                    NS_GET_IID(nsIMimeConverter), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->DecodeMimePartIIStr(header, charset, decodedString, eatContinuations);
    NS_RELEASE(converter);
  }
  return res;
}

// Get a default mail character set.
char * nsMsgI18NGetDefaultMailCharset()
{
  nsresult res = NS_OK;
  char * retVal = nsnull;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (nsnull != prefs && NS_SUCCEEDED(res))
  {
      char *prefValue;
	  res = prefs->CopyCharPref("mailnews.send_default_charset", &prefValue);
	  
	  if (NS_SUCCEEDED(res)) 
	  {
  		//TODO: map to mail charset (e.g. Shift_JIS -> ISO-2022-JP) bug#3941.
  		 retVal = prefValue;
	  }
	  else 
		  retVal = nsCRT::strdup("ISO-8859-1");
  }

  return (nsnull != retVal) ? retVal : nsCRT::strdup("ISO-8859-1");
}

// Return True if a charset is stateful (e.g. JIS).
PRBool nsMsgI18Nstateful_charset(const char *charset)
{
  //TODO: use charset manager's service
  return (nsCRT::strcasecmp(charset, "ISO-2022-JP") == 0);
}

// Check 7bit in a given buffer.
// This is expensive (both memory and performance).
// The check would be very simple if applied to an unicode text (e.g. nsString or utf-8).
// Possible optimazaion is to search ESC(0x1B) in case of iso-2022-jp and iso-2022-kr.
// Or convert and check line by line.
PRBool nsMsgI18N7bit_data_part(const char *charset, const char *inString, const PRUint32 size)
{
  char *aCString;
  nsAutoString aCharset(charset);
  nsAutoString outString;
  nsresult res;
  
  aCString = (char *) PR_Malloc(size + 1);
  if (nsnull != aCString) {
    PL_strncpy(aCString, inString, size); // make a C string
    res = ConvertToUnicode(aCharset, aCString, outString);
    PR_Free(aCString);
    if (NS_SUCCEEDED(res)) {
      for (PRInt32 i = 0; i < outString.Length(); i++) {
        if (outString.CharAt(i) > 127) {
          return PR_FALSE;
        }
      }
    }
  }
  return PR_TRUE;  // all 7 bit
}

// Simple parser to parse META charset. 
// It only supports the case when the description is within one line. 
const char * 
nsMsgI18NParseMetaCharset(nsFileSpec* fileSpec) 
{ 
  static char charset[kMAX_CSNAME+1]; 
  char buffer[512]; 
  nsInputFileStream fileStream(*fileSpec); 

  *charset = '\0'; 

  while (!fileStream.eof() && !fileStream.failed() && 
         fileStream.is_open()) { 
    fileStream.readline(buffer, 512); 
    if (*buffer == CR || *buffer == LF || *buffer == 0) 
      continue; 

    for (int i = 0; i < (int)PL_strlen(buffer); i++) { 
      buffer[i] = toupper(buffer[i]); 
    } 

    if (PL_strstr(buffer, "/HEAD")) 
      break; 

    if (PL_strstr(buffer, "META") && 
        PL_strstr(buffer, "HTTP-EQUIV") && 
        PL_strstr(buffer, "CONTENT-TYPE") && 
        PL_strstr(buffer, "CHARSET") 
        ) 
    { 
      char *cp = PL_strstr(PL_strstr(buffer, "CHARSET"), "=") + 1; 
      char seps[]   = " \"\'"; 
      char *token; 
      char* newStr; 
      token = nsCRT::strtok(cp, seps, &newStr); 
      if (token != NULL) 
      { 
        PL_strcpy(charset, token); 
      } 
    } 
  } 

  return charset; 
} 

nsresult nsMsgI18NConvertToEntity(const nsString& inString, nsString* outString)
{
  nsresult res;

  outString->Truncate();
  nsCOMPtr <nsIEntityConverter> entityConv;
  res = nsComponentManager::CreateInstance(kEntityConverterCID, NULL, 
                                           NS_GET_IID(nsIEntityConverter), getter_AddRefs(entityConv));
  if(NS_SUCCEEDED(res)) {
    PRUnichar *entities = NULL;
    res = entityConv->ConvertToEntities(inString.GetUnicode(), nsIEntityConverter::html40Latin1, &entities);
    if (NS_SUCCEEDED(res) && (NULL != entities)) {
      outString->Assign(entities);
      nsAllocator::Free(entities);
     }
   }
 
  return res;
}

nsresult nsMsgI18NSaveAsCharset(const char* contentType, const char *charset, const PRUnichar* inString, char** outString)
{
  NS_ASSERTION(contentType, "null ptr- contentType");
  NS_ASSERTION(charset, "null ptr- charset");
  NS_ASSERTION(outString, "null ptr- outString");
  if(!contentType || !charset || !inString || !outString)
    return NS_ERROR_NULL_POINTER;
  if (0 == *inString) {
    *outString = nsCRT::strdup("");
    return (NULL != *outString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
  *outString = NULL;

  PRBool bTEXT_HTML = PR_FALSE;
  nsresult res;

  if (!nsCRT::strcasecmp(contentType, TEXT_HTML)) {
    bTEXT_HTML = PR_TRUE;
  }
  else if (nsCRT::strcasecmp(contentType, TEXT_PLAIN)) {
    return NS_ERROR_ILLEGAL_VALUE;  // not supported type
  }

  // Resolve charset alias
  nsAutoString aCharset(charset);
  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, aCharset);
    }
  }
  if (NS_FAILED(res)) {
    return res;
  }

  nsCOMPtr <nsISaveAsCharset> aConv;  // charset converter plus entity, NCR generation
  res = nsComponentManager::CreateInstance(kSaveAsCharsetCID, NULL, 
                                           NS_GET_IID(nsISaveAsCharset), getter_AddRefs(aConv));
  if(NS_SUCCEEDED(res)) {
    // attribute: 
    // html text - charset conv then fallback to entity or NCR
    // plain text - charset conv then fallback to '?'
    char charset_buf[kMAX_CSNAME+1];
    if (bTEXT_HTML) {
      res = aConv->Init(aCharset.ToCString(charset_buf, kMAX_CSNAME+1), 
                        nsISaveAsCharset::attr_htmlTextDefault, 
                        nsIEntityConverter::html40);
    }
    else {
      // fallback for text/plain: first try transliterate then '?'
      res = aConv->Init(aCharset.ToCString(charset_buf, kMAX_CSNAME+1), 
                        nsISaveAsCharset::attr_FallbackQuestionMark + nsISaveAsCharset::attr_EntityAfterCharsetConv, 
                        nsIEntityConverter::transliterate);
    }
    if (NS_SUCCEEDED(res)) {
      // Mapping characters in a certain range (required for Japanese only)
      if (!nsCRT::strcasecmp("ISO-2022-JP", charset)) {
        NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
        if (nsnull != prefs && NS_SUCCEEDED(res)) {
          PRBool return_val;

          if (NS_FAILED(prefs->GetBoolPref("mailnews.send_hankaku_kana", &return_val)))
            return_val = PR_FALSE;  // no pref means need the mapping

          if (!return_val) {
            nsCOMPtr <nsITextTransform> textTransform;
            res = nsComponentManager::CreateInstance(NS_HANKAKUTOZENKAKU_PROGID, nsnull, 
                                                     NS_GET_IID(nsITextTransform), getter_AddRefs(textTransform));
            if (NS_SUCCEEDED(res)) {
              nsAutoString aText(inString);
              nsAutoString aResult;
              res = textTransform->Change(aText, aResult);
              if (NS_SUCCEEDED(res)) {
                return aConv->Convert(aResult.GetUnicode(), outString);
              }
            }
          }
        }
      }

      // Convert to charset
      res = aConv->Convert(inString, outString);
    }
  }
 
  return res;
}

// RICHIE - not sure about this one?? need to see what it did in the old
// world.
char *
nsMsgI18NGetAcceptLanguage(void)
{
  return "en";
}
