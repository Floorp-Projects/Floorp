/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */ 

// as does this
#define NS_IMPL_IDS
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "nsISupports.h"
#include "nsIPref.h"
#include "nsIMimeConverter.h"
#include "msgCore.h"
#include "rosetta_mailnews.h"
#include "nsMsgI18N.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//
// International functions necessary for composition
//

// Convert an unicode string to a C string with a given charset.
nsresult ConvertFromUnicode(const nsString& aCharset, 
                            const nsString& inString,
                            char** outCString)
{
  nsresult res;

  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeEncoder* encoder = nsnull;
    nsString convCharset;

    // map to converter charset
    if (aCharset.EqualsIgnoreCase("us-ascii")) {
      convCharset.SetString("iso-8859-1");
    }
    else {
      convCharset = aCharset; 
    }

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&convCharset, &encoder);
    if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
      const PRUnichar *unichars = inString.GetUnicode();
      PRInt32 unicharLength = inString.Length();
      PRInt32 dstLength;
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      // allocale an output buffer
      *outCString = (char *) PR_Malloc(dstLength + 1);
      if (*outCString != nsnull) {
        PRInt32 originalLength = unicharLength;
        // convert from unicode
        res = encoder->Convert(unichars, &unicharLength, *outCString, &dstLength);
        // estimation of GetMaxLength was incorrect
        if (unicharLength < originalLength) {
          PR_Free(*outCString);
          res = NS_ERROR_FAILURE;
        }
        else {
          (*outCString)[dstLength] = '\0';
        }
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
  nsresult res;
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeDecoder* decoder = nsnull;
    PRUnichar *unichars;
    PRInt32 unicharLength;
    nsString convCharset;

    // map to converter charset
    if (aCharset.EqualsIgnoreCase("us-ascii")) {
      convCharset.SetString("iso-8859-1");
    }
    else {
      convCharset = aCharset; 
    }
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
        outString.SetString(unichars, unicharLength);
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
                                           nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->EncodeMimePartIIStr_UTF8(header, charset, kMIME_ENCODED_WORD_SIZE, &encodedString);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? encodedString : nsnull;
}

// MIME decoder
nsresult nsMsgI18NDecodeMimePartIIStr(const nsString& header, nsString& charset, nsString& decodedString)
{
  nsIMimeConverter *converter;
  nsresult res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                                    nsCOMTypeInfo<nsIMimeConverter>::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->DecodeMimePartIIStr(header, charset, decodedString);
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
	  res = prefs->CopyCharPref("intl.character_set_name", &prefValue);
	  
	  if (NS_SUCCEEDED(res)) 
	  {
  		//TODO: map to mail charset (e.g. Shift_JIS -> ISO-2022-JP) bug#3941.
  		 retVal = prefValue;
	  }
	  else 
		  retVal = PL_strdup("iso-8859-1");
  }

  return (nsnull != retVal) ? retVal : PL_strdup("iso-8859-1");
}

// Return True if a charset is stateful (e.g. JIS).
PRBool nsMsgI18Nstateful_charset(const char *charset)
{
  //TODO: use charset manager's service
  return (PL_strcasecmp(charset, "iso-2022-jp") == 0);
}

// Check 7bit in a given buffer.
// This is expensive (both memory and performance).
// The check would be very simple if applied to an unicode text (e.g. nsString or utf-8).
// Possible optimazaion is to search ESC(0x1B) in case of iso-2022-jp and iso-2022-kr.
// Or convert and check line by line.
PRBool nsMsgI18N7bit_data_part(const char *charset, const char *inString, const PRUint32 size)
{
  char *aCString;
  nsString aCharset(charset);
  nsString outString;
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
  static char charset[65]; 
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

// RICHIE - not sure about this one?? need to see what it did in the old
// world.
char *
nsMsgI18NGetAcceptLanguage(void)
{
  return "en";
}
