/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMsgUtf7Utils.h"

#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);


// convert back and forth between imap utf7 and unicode.
char*
CreateUtf7ConvertedString(const char * aSourceString, 
                      PRBool aConvertToUtf7Imap)
{
  nsresult res;
  char *dstPtr = nsnull;
  PRInt32 dstLength = 0;
  char *convertedString = NULL;
  
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    if (!aConvertToUtf7Imap)
    {
      // convert utf7 to unicode
      nsIUnicodeDecoder* decoder = nsnull;

      res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
      if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
      {
        PRInt32 srcLen = PL_strlen(aSourceString);
        res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
        // temporary buffer to hold unicode string
        unichars = new PRUnichar[unicharLength + 1];
        if (unichars == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
          res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
          unichars[unicharLength] = 0;
        }
        NS_IF_RELEASE(decoder);
        // convert the unicode to 8 bit ascii.
        nsString unicodeStr(unichars);
        convertedString = (char *) PR_Malloc(unicharLength + 1);
        if (convertedString)
          unicodeStr.ToCString(convertedString, unicharLength + 1, 0);
      }
    }
    else
    {
      // convert from 8 bit ascii string to modified utf7
      nsString unicodeStr; unicodeStr.AssignWithConversion(aSourceString);
      nsIUnicodeEncoder* encoder = nsnull;
      aCharset.AssignWithConversion("x-imap4-modified-utf7");
      res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder)) 
      {
        res = encoder->GetMaxLength(unicodeStr.GetUnicode(), unicodeStr.Length(), &dstLength);
        // allocale an output buffer
        dstPtr = (char *) PR_CALLOC(dstLength + 1);
        unicharLength = unicodeStr.Length();
        if (dstPtr == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
          res = encoder->Convert(unicodeStr.GetUnicode(), &unicharLength, dstPtr, &dstLength);
          dstPtr[dstLength] = 0;
        }
      }
      NS_IF_RELEASE(encoder);
      nsString unicodeStr2; unicodeStr2.AssignWithConversion(dstPtr);
      convertedString = (char *) PR_Malloc(dstLength + 1);
      if (convertedString)
        unicodeStr2.ToCString(convertedString, dstLength + 1, 0);
        }
        delete [] unichars;
      }
    PR_FREEIF(dstPtr);
    return convertedString;
}

// convert back and forth between imap utf7 and unicode.
char*
CreateUtf7ConvertedStringFromUnicode(const PRUnichar * aSourceString)
{
  nsresult res;
  char *dstPtr = nsnull;
  PRInt32 dstLength = 0;
  
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRInt32 unicharLength;

      // convert from 8 bit ascii string to modified utf7
      nsString unicodeStr(aSourceString);
      nsIUnicodeEncoder* encoder = nsnull;
      aCharset.AssignWithConversion("x-imap4-modified-utf7");
      res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder)) 
      {
        res = encoder->GetMaxLength(unicodeStr.GetUnicode(), unicodeStr.Length(), &dstLength);
        // allocale an output buffer
        dstPtr = (char *) PR_CALLOC(dstLength + 1);
        unicharLength = unicodeStr.Length();
        if (dstPtr == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
			// this should be enough of a finish buffer - utf7 isn't changing, and it'll always be '-'
		  char finishBuffer[20];
		  PRInt32 finishSize = sizeof(finishBuffer);

          res = encoder->Convert(unicodeStr.GetUnicode(), &unicharLength, dstPtr, &dstLength);
		  encoder->Finish(finishBuffer, &finishSize);
		  finishBuffer[finishSize] = '\0';
          dstPtr[dstLength] = 0;
		  strcat(dstPtr, finishBuffer);
        }
      }
      NS_IF_RELEASE(encoder);
        }
    return dstPtr;
}


nsresult CreateUnicodeStringFromUtf7(const char *aSourceString, PRUnichar **aUnicodeStr)
{
  if (!aUnicodeStr)
	  return NS_ERROR_NULL_POINTER;

  PRUnichar *convertedString = NULL;
  nsresult res;
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    // convert utf7 to unicode
    nsIUnicodeDecoder* decoder = nsnull;

    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
    {
      PRInt32 srcLen = PL_strlen(aSourceString);
      res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
      // temporary buffer to hold unicode string
      unichars = new PRUnichar[unicharLength + 1];
      if (unichars == nsnull) 
      {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else 
      {
        res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
        unichars[unicharLength] = 0;
      }
      NS_IF_RELEASE(decoder);
      nsString unicodeStr(unichars);
      convertedString = unicodeStr.ToNewUnicode();
	  delete [] unichars;
    }
  }
  *aUnicodeStr = convertedString;
  return (convertedString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

