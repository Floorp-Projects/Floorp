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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "stdio.h"
#include "mimecom.h"
#include "modmimee.h"
#include "nscore.h"
#include "nsMimeConverter.h"
#include "comi18n.h"
#include "nsMsgI18N.h"
#include "prmem.h"
#include "plstr.h"

NS_IMPL_ISUPPORTS1(nsMimeConverter, nsIMimeConverter)

/*
 * nsMimeConverter definitions....
 */

/* 
 * Inherited methods for nsMimeConverter
 */
nsMimeConverter::nsMimeConverter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_ISUPPORTS();
}

nsMimeConverter::~nsMimeConverter()
{
}

nsresult 
nsMimeConverter::DecodeMimePartIIStr(const nsCString& header, 
                                           nsCString& charset, 
                                           nsString& decodedString,
                                     PRBool eatContinuations)
{
  char charsetNameBuffer[kMAX_CSNAME+1];
  char *decodedCstr = nsnull;
  nsresult res = NS_OK;

  // initialize the charset buffer
  PL_strcpy(charsetNameBuffer, "us-ascii");

  // apply MIME decode.
  decodedCstr = MIME_DecodeMimePartIIStr(header,
                                           charsetNameBuffer, eatContinuations);
  if (nsnull == decodedCstr) {
    // no decode needed and no default charset was specified
    if (charset.IsEmpty()) {
      decodedString.AssignWithConversion(header);
    }
    else {
      // no MIME encoded, convert default charset to unicode
      res = ConvertToUnicode(NS_ConvertASCIItoUCS2(charset), header, decodedString);
    }
  }
  else {
    // assign the charset encoded in MIME header
    charset.Assign(charsetNameBuffer);
    // convert MIME charset to unicode
    res = ConvertToUnicode(NS_ConvertASCIItoUCS2(charset), (const char *) decodedCstr, decodedString);
    PR_FREEIF(decodedCstr);
  }
  return res;
}

nsresult 
nsMimeConverter::DecodeMimePartIIStr(const nsString& header, 
                                           nsString& charset, 
                                           PRUnichar **decodedString,
                                     PRBool eatContinuations)
{
  char charsetCstr[kMAX_CSNAME+1];
  char *decodedCstr = nsnull;
  nsresult res = NS_OK;

  (void) charset.ToCString(charsetCstr, kMAX_CSNAME+1);
  nsCAutoString encodedStr; encodedStr.AssignWithConversion(header);
  // apply MIME decode.
  decodedCstr = MIME_DecodeMimePartIIStr(encodedStr,
                                           charsetCstr, eatContinuations);
  if (nsnull == decodedCstr) {
    // no decode needed and no default charset was specified
    if (*charsetCstr == '\0') {
      *decodedString = header.ToNewUnicode();
    }
    else {
      // no MIME encoded, convert default charset to unicode
      nsAutoString decodedStr;
      res = ConvertToUnicode(charset, encodedStr, decodedStr);
      *decodedString = decodedStr.ToNewUnicode();
    }
  }
  else {
    // convert MIME charset to unicode
    nsAutoString decodedStr;
    res = ConvertToUnicode(NS_ConvertASCIItoUCS2(charsetCstr), (const char *) decodedCstr, decodedStr);
    *decodedString = decodedStr.ToNewUnicode();
    PR_FREEIF(decodedCstr);
  }
  return res;
}
 
nsresult
nsMimeConverter::DecodeMimePartIIStr(const char *header, 
                                           char       *charset, 
                                           char **decodedString,
                                     PRBool eatContinuations)
{
  char *retString = MIME_DecodeMimePartIIStr(header, charset, eatContinuations); 
  if (retString == NULL)
    return NS_ERROR_FAILURE;
  else
  {
    *decodedString = retString;
    return NS_OK;
  }
}

nsresult
nsMimeConverter::EncodeMimePartIIStr(const char    *header, 
                                           const char    *mailCharset, 
                                           const PRInt32 encodedWordSize, 
                                           char          **encodedString)
{
  char *utf8String;

  // Encoder needs utf-8 string.
  if (MIME_ConvertString(mailCharset, "UTF-8", header, &utf8String) != 0)
    return NS_ERROR_FAILURE;

  nsresult rv = EncodeMimePartIIStr_UTF8((const char *) utf8String, mailCharset, encodedWordSize, encodedString);
  nsCRT::free(utf8String);
  return rv;
}

nsresult
nsMimeConverter::EncodeMimePartIIStr_UTF8(const char    *header, 
                                          const char    *mailCharset, 
                                          const PRInt32 encodedWordSize, 
                                          char          **encodedString)
{
  char *retString = MIME_EncodeMimePartIIStr(header, mailCharset, encodedWordSize);
  if (retString == NULL)
    return NS_ERROR_FAILURE;
  else
  {
    *encodedString = retString;
    return NS_OK;
  }
}

// Apply charset conversion to a given buffer. The conversion is done by an unicode round trip.
nsresult
nsMimeConverter::ConvertCharset(const PRBool autoDetection, const char* from_charset, const char* to_charset,
                                const char* inBuffer, const PRInt32 inLength, char** outBuffer, PRInt32* outLength,
                                PRInt32* numUnConverted)
{
  return MIME_ConvertCharset(autoDetection, from_charset, to_charset, inBuffer, inLength, 
                             outBuffer, outLength, numUnConverted);
}


nsresult 
nsMimeConverter::B64EncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure, 
                   MimeEncoderData **returnEncoderData) 
{
MimeEncoderData   *ptr;

  ptr = MimeB64EncoderInit(output_fn, closure);
  if (ptr)
  {
    *returnEncoderData = ptr;
    return NS_OK;
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsMimeConverter::QPEncoderInit (int (*output_fn) (const char *buf, 
                      PRInt32 size, void *closure), void *closure, MimeEncoderData ** returnEncoderData) 
{
MimeEncoderData   *ptr;

  ptr = MimeQPEncoderInit(output_fn, closure);
  if (ptr)
  {
    *returnEncoderData = ptr;
    return NS_OK;
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsMimeConverter::UUEncoderInit (char *filename, int (*output_fn) 
                     (const char *buf, PRInt32 size, void *closure), void *closure, 
                     MimeEncoderData ** returnEncoderData)
{
MimeEncoderData   *ptr;

  ptr = MimeUUEncoderInit(filename, output_fn, closure);
  if (ptr)
  {
    *returnEncoderData = ptr;
    return NS_OK;
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsMimeConverter::EncoderDestroy(MimeEncoderData *data, PRBool abort_p) 
{
  MimeEncoderDestroy(data, abort_p);
  return NS_OK;
}

nsresult
nsMimeConverter::EncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size, PRInt32 *written) 
{
  PRInt32   writeCount;
  writeCount = MimeEncoderWrite(data, buffer, size);
  *written = writeCount;
  return NS_OK;
}
