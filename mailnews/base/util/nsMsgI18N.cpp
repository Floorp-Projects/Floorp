/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// as does this
#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager2.h"

#include "nsISupports.h"
#include "nsIPref.h"
#include "nsIMimeConverter.h"
#include "msgCore.h"
#include "nsMsgI18N.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsMsgMimeCID.h"
#include "nsMimeTypes.h"
#include "nsIEntityConverter.h"
#include "nsISaveAsCharset.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prmem.h"
#include "nsFileSpec.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

//
// International functions necessary for composition
//

nsresult nsMsgI18NConvertFromUnicode(const nsCString& aCharset, 
                                     const nsString& inString,
                                     nsCString& outString)
{
  if (inString.IsEmpty()) {
    outString.Truncate(0);
    return NS_OK;
  }
  // Note: this will hide a possible error when the unicode text may contain more than one charset.
  // (e.g. Latin1 + Japanese). Use nsMsgI18NSaveAsCharset instead to avoid that problem.
  else if (aCharset.IsEmpty() ||
      aCharset.EqualsIgnoreCase("us-ascii") ||
      aCharset.EqualsIgnoreCase("ISO-8859-1")) {
    outString.AssignWithConversion(inString);
    return NS_OK;
  }
  else if (aCharset.EqualsIgnoreCase("UTF-8")) {
    char *s = ToNewUTF8String(inString);
    if (NULL == s)
      return NS_ERROR_OUT_OF_MEMORY;
    outString.Assign(s);
    Recycle(s);
    return NS_OK;
  }
  nsAutoString convCharset; convCharset.AssignWithConversion("ISO-8859-1");
  nsresult res;

  // Resolve charset alias
  nsCOMPtr <nsICharsetAlias> calias = do_GetService(NS_CHARSETALIAS_CONTRACTID, &res);
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias; aAlias.AssignWithConversion(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }

  nsCOMPtr <nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if(NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIUnicodeEncoder> encoder;

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&convCharset, getter_AddRefs(encoder));
    if(NS_SUCCEEDED(res)) {
      res = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
      if (NS_SUCCEEDED(res)) {

        const PRUnichar *originalSrcPtr = inString.get();
        PRUnichar *currentSrcPtr = NS_CONST_CAST(PRUnichar *, originalSrcPtr);
        PRInt32 originalUnicharLength = inString.Length();
        PRInt32 srcLength;
        PRInt32 dstLength;
        char localbuf[512];
        PRInt32 consumedLen = 0;

        outString.Assign("");

        // convert
        while (consumedLen < originalUnicharLength) {
          srcLength = originalUnicharLength - consumedLen;  
          dstLength = 512;
          res = encoder->Convert(currentSrcPtr, &srcLength, localbuf, &dstLength);
          if (NS_FAILED(res) || dstLength == 0)
            break;
          outString.Append(localbuf, dstLength);

          currentSrcPtr += srcLength;
          consumedLen = currentSrcPtr - originalSrcPtr; // src length used so far
        }
        res = encoder->Finish(localbuf, &dstLength);
        if (NS_SUCCEEDED(res))
          outString.Append(localbuf, dstLength);
      }
    }    
  }
  return res;
}

nsresult nsMsgI18NConvertToUnicode(const nsCString& aCharset, 
                                   const nsCString& inString, 
                                   nsString& outString)
{
  if (inString.IsEmpty()) {
    outString.Truncate();
    return NS_OK;
  }
  else if (aCharset.IsEmpty() ||
      aCharset.EqualsIgnoreCase("us-ascii") ||
      aCharset.EqualsIgnoreCase("ISO-8859-1")) {
    outString.AssignWithConversion(inString);
    return NS_OK;
  }

  nsAutoString convCharset;
  nsresult res;

  // Resolve charset alias
  nsCOMPtr <nsICharsetAlias> calias = do_GetService(NS_CHARSETALIAS_CONTRACTID, &res);
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias; aAlias.AssignWithConversion(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }
  if (NS_FAILED(res)) {
    return res;
  }

  nsCOMPtr <nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if(NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIUnicodeDecoder> decoder;

    // get an unicode converter
    res = ccm->GetUnicodeDecoder(&convCharset, getter_AddRefs(decoder));
    if(NS_SUCCEEDED(res)) {

      const char *originalSrcPtr = inString.get();
      char *currentSrcPtr = NS_CONST_CAST(char *, originalSrcPtr);
      PRInt32 originalLength = inString.Length();
      PRInt32 srcLength;
      PRInt32 dstLength;
      PRUnichar localbuf[512];
      PRInt32 consumedLen = 0;

      outString.AssignWithConversion("");

      // convert
      while (consumedLen < originalLength) {
        srcLength = originalLength - consumedLen;  
        dstLength = 512;
        res = decoder->Convert(currentSrcPtr, &srcLength, localbuf, &dstLength);
        if (NS_FAILED(res) || dstLength == 0)
          break;
        outString.Append(localbuf, dstLength);

        currentSrcPtr += srcLength;
        consumedLen = currentSrcPtr - originalSrcPtr; // src length used so far
      }
    }    
  }  
  return res;
}

// Convert an unicode string to a C string with a given charset.
nsresult ConvertFromUnicode(const nsString& aCharset, 
                            const nsString& inString,
                            char** outCString)
{
#if 0 
  nsCAutoString s;
  nsresult rv;
  rv = nsMsgI18NConvertFromUnicode(aCharset, inString, s);
  *outCString = PL_strdup(s);
  return rv;
#endif

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
    *outCString = ToNewCString(inString);
    return (NULL == *outCString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }
  else if (aCharset.EqualsIgnoreCase("UTF-8")) {
    *outCString = ToNewUTF8String(inString);
    return (NULL == *outCString) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
  }

  nsAutoString convCharset; convCharset.AssignWithConversion("ISO-8859-1");
  nsresult res;

  // Resolve charset alias
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res)); 
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }

  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res)) {
    nsIUnicodeEncoder* encoder = nsnull;

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&convCharset, &encoder);
    if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
      PRUnichar *unichars = (PRUnichar *) inString.get();
      PRInt32 unicharLength = inString.Length();
      PRInt32 dstLength;
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      if (NS_SUCCEEDED(res)) {
        res = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
        if (NS_SUCCEEDED(res)) {
          // allocale an output buffer
          *outCString = (char *) PR_Malloc(dstLength + 1);
          if (nsnull != *outCString) {
            PRInt32 buffLength = dstLength;
            **outCString = '\0';
            res = encoder->Convert(unichars, &unicharLength, *outCString, &dstLength);
            if (NS_SUCCEEDED(res)) {
              PRInt32 finLen = buffLength - dstLength;
              res = encoder->Finish((char *)(*outCString+dstLength), &finLen);
              if (NS_SUCCEEDED(res)) {
                dstLength += finLen;
              }
              (*outCString)[dstLength] = '\0';
            }
          }
          else {
            res = NS_ERROR_OUT_OF_MEMORY;
          }
        }
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
#if 0 
  nsresult rv;
  rv = nsMsgI18NConvertToUnicode(aCharset, nsCAutoString(inCString), outString);
  return rv;
#endif

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
    outString.AssignWithConversion(inCString);
    return NS_OK;
  }

  nsAutoString convCharset;
  nsresult res;

  // Resolve charset alias
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res)); 
  if (NS_SUCCEEDED(res)) {
    nsAutoString aAlias(aCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, convCharset);
    }
  }
  if (NS_FAILED(res)) {
    // ignore charset alias error and use fallback charset.
    convCharset = NS_LITERAL_STRING("ISO-8859-1").get();
    res = NS_OK;
  }

  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(kCharsetConverterManagerCID, &res); 

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
const nsString& nsMsgI18NFileSystemCharset()
{
	/* Get a charset used for the file. */
	static nsString aPlatformCharset;

	if (aPlatformCharset.Length() < 1) 
	{
		nsresult rv;
		nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
		if (NS_SUCCEEDED(rv)) 
			rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aPlatformCharset);

		if (NS_FAILED(rv)) 
			aPlatformCharset.AssignWithConversion("ISO-8859-1");
	}
	return aPlatformCharset;
}

// MIME encoder, output string should be freed by PR_FREE
char * nsMsgI18NEncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime) 
{
  // No MIME, convert to the outgoing mail charset.
  if (PR_FALSE == bUseMime) {
    char *convertedStr;
    if (NS_SUCCEEDED(ConvertFromUnicode(NS_ConvertASCIItoUCS2(charset), NS_ConvertUTF8toUCS2(header), &convertedStr)))
      return (convertedStr);
    else
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

// Get a default mail character set.
char * nsMsgI18NGetDefaultMailCharset()
{
  nsresult res = NS_OK;
  char * retVal = nsnull;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res)); 
  if (nsnull != prefs && NS_SUCCEEDED(res))
  {
    nsXPIDLString prefValue;
    res = prefs->GetLocalizedUnicharPref("mailnews.send_default_charset", getter_Copies(prefValue));

    if (NS_SUCCEEDED(res))
    {
      //TODO: map to mail charset (e.g. Shift_JIS -> ISO-2022-JP) bug#3941.
      retVal = ToNewUTF8String(prefValue);
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

PRBool nsMsgI18Nmultibyte_charset(const char *charset)
{
  nsresult res;
  nsCOMPtr <nsICharsetConverterManager2> ccm2 = do_GetService(kCharsetConverterManagerCID, &res);
  PRBool result = PR_FALSE;

  if (NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIAtom> charsetAtom;
    nsAutoString charsetData;
    res = ccm2->GetCharsetAtom(NS_ConvertASCIItoUCS2(charset).get(), getter_AddRefs(charsetAtom));
    if (NS_SUCCEEDED(res)) {
      res = ccm2->GetCharsetData2(charsetAtom, NS_ConvertASCIItoUCS2(".isMultibyte").get(), &charsetData);
      if (NS_SUCCEEDED(res)) {
        result = charsetData.EqualsWithConversion("true", PR_TRUE);
      }
    }
  }

  return result;
}

// Check 7bit in a given buffer.
// This is expensive (both memory and performance).
// The check would be very simple if applied to an unicode text (e.g. nsString or utf-8).
// Possible optimazaion is to search ESC(0x1B) in case of iso-2022-jp and iso-2022-kr.
// Or convert and check line by line.
PRBool nsMsgI18N7bit_data_part(const char *charset, const char *inString, const PRUint32 size)
{
  nsAutoString aCharset; aCharset.AssignWithConversion(charset);
  nsresult res;
  PRBool result = PR_TRUE;
  
  nsCOMPtr <nsICharsetConverterManager> ccm = do_GetService(kCharsetConverterManagerCID, &res);

  if (NS_SUCCEEDED(res)) {
    nsIUnicodeDecoder* decoder = nsnull;

    // get an unicode converter
    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res)) {
      char *currentSrcPtr = NS_CONST_CAST(char *, inString);
      PRUint32 consumedLen = 0;
      PRUnichar unicharBuff[512];
      PRInt32 srcLen;
      PRInt32 unicharLength;

      // convert to unicode
      while (consumedLen < size) {
        srcLen = ((size - consumedLen) >= 512) ? 512 : (size - consumedLen);  // buffer len or remaining src len
        unicharLength = 512;
        res = decoder->Convert(currentSrcPtr, &srcLen, unicharBuff, &unicharLength);
        if (NS_FAILED(res))
          break;
        for (PRInt32 i = 0; i < unicharLength; i++) {
          if (unicharBuff[i] > 127) {
            result = PR_FALSE;
            break;
          }
        }
        currentSrcPtr += srcLen;
        consumedLen = currentSrcPtr - inString; // src length used so far
      }
      NS_IF_RELEASE(decoder);
    }    
  }

  return result;
}

PRBool nsMsgI18Ncheck_data_in_charset_range(const char *charset, const PRUnichar* inString)
{
  if (!charset || !*charset || !inString || !*inString)
    return PR_TRUE;

  nsAutoString aCharset; aCharset.AssignWithConversion(charset);
  nsresult res;
  PRBool result = PR_TRUE;
  
  nsCOMPtr <nsICharsetConverterManager> ccm = do_GetService(kCharsetConverterManagerCID, &res);

  if (NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIUnicodeEncoder> encoder;

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&aCharset, getter_AddRefs(encoder));
    if(NS_SUCCEEDED(res)) {
      const PRUnichar *originalPtr = inString;
      PRInt32 originalLen = nsCRT::strlen(inString);
      PRUnichar *currentSrcPtr = NS_CONST_CAST(PRUnichar *, originalPtr);
      char localBuff[512];
      PRInt32 consumedLen = 0;
      PRInt32 srcLen;
      PRInt32 dstLength;

      // convert from unicode
      while (consumedLen < originalLen) {
        srcLen = originalLen - consumedLen;
        dstLength = 512;
        res = encoder->Convert(currentSrcPtr, &srcLen, localBuff, &dstLength);
        if (NS_ERROR_UENC_NOMAPPING == res) {
          result = PR_FALSE;
          break;
        }
        else if (NS_FAILED(res) || (0 == dstLength))
          break;

        currentSrcPtr += srcLen;
        consumedLen = currentSrcPtr - originalPtr; // src length used so far
      }
    }    
  }

  return result;
}

// Simple parser to parse META charset. 
// It only supports the case when the description is within one line. 
const char * 
nsMsgI18NParseMetaCharset(nsFileSpec* fileSpec) 
{ 
  static char charset[kMAX_CSNAME+1]; 
  char buffer[512]; 

  *charset = '\0'; 

  if (fileSpec->IsDirectory()) {
    NS_ASSERTION(0,"file is a directory");
    return charset; 
  }

  nsInputFileStream fileStream(*fileSpec); 

  while (!fileStream.eof() && !fileStream.failed() && 
         fileStream.is_open()) { 
    fileStream.readline(buffer, 512); 
    if (*buffer == nsCRT::CR || *buffer == nsCRT::LF || *buffer == 0) 
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
    res = entityConv->ConvertToEntities(inString.get(), nsIEntityConverter::html40Latin1, &entities);
    if (NS_SUCCEEDED(res) && (NULL != entities)) {
      outString->Assign(entities);
      nsMemory::Free(entities);
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
  nsAutoString aCharset; aCharset.AssignWithConversion(charset);
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res)); 
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
      // For ISO-8859-1 only, convert to entity first (always generate entites like &nbsp;).
      res = aConv->Init(aCharset.ToCString(charset_buf, kMAX_CSNAME+1), 
                        aCharset.EqualsIgnoreCase("ISO-8859-1") ?
                          nsISaveAsCharset::attr_htmlTextDefault :
                          nsISaveAsCharset::attr_EntityAfterCharsetConv + nsISaveAsCharset::attr_FallbackDecimalNCR, 
                        nsIEntityConverter::html32);
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
        nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res)); 
        if (nsnull != prefs && NS_SUCCEEDED(res)) {
          PRBool return_val;

          if (NS_FAILED(prefs->GetBoolPref("mailnews.send_hankaku_kana", &return_val)))
            return_val = PR_FALSE;  // no pref means need the mapping

          if (!return_val) {
            nsCOMPtr <nsITextTransform> textTransform;
            res = nsComponentManager::CreateInstance(NS_HANKAKUTOZENKAKU_CONTRACTID, nsnull, 
                                                     NS_GET_IID(nsITextTransform), getter_AddRefs(textTransform));
            if (NS_SUCCEEDED(res)) {
              nsAutoString aText(inString);
              nsAutoString aResult;
              res = textTransform->Change(aText, aResult);
              if (NS_SUCCEEDED(res)) {
                return aConv->Convert(aResult.get(), outString);
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

nsresult nsMsgI18NFormatNNTPXPATInNonRFC1522Format(const nsCString& aCharset, 
                                                   const nsString& inString, 
                                                   nsCString& outString)
{
  outString.AssignWithConversion(inString);
  return NS_OK;
}

char *
nsMsgI18NGetAcceptLanguage(void)
{
  static char   lang[32];
  nsresult      res;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res)); 
  if (nsnull != prefs && NS_SUCCEEDED(res))
  {
    nsXPIDLString prefValue;

    nsCRT::memset(lang, 0, sizeof(lang));
    res = prefs->GetLocalizedUnicharPref("intl.accept_languages", getter_Copies(prefValue));
	  if (NS_SUCCEEDED(res) && prefValue) 
    {
      PL_strncpy(lang, NS_ConvertUCS2toUTF8(prefValue).get(), sizeof(lang));
    }
	  else 
		  PL_strcpy(lang, "en");
  }
  else
    PL_strcpy(lang, "en");

  return (char *)lang;
}


