/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
 
/*
 * This interface allows any module to access the encoder/decoder 
 * routines for RFC822 headers, base64 and QP encoders. This will 
 * allow any mail/news module to call on these routines.
 */
#ifndef nsIMimeConverter_h_
#define nsIMimeConverter_h_

#include "prtypes.h"
#include "nsString.h"

// {ea5b631e-1dd1-11b2-be0d-e02825f300d0}
#define NS_IMIME_CONVERTER_IID \
      { 0xea5b631e, 0x1dd1, 0x11b2,   \
      { 0xbe, 0x0d, 0xe0, 0x28, 0x25, 0xf3, 0x00, 0xd0 } }

// default line length for calling the encoder
#define kMIME_ENCODED_WORD_SIZE       72 

// Max length of charset name. 
#define kMAX_CSNAME                   64 

/* Opaque objects used by the encoder/decoder to store state. */
typedef struct MimeDecoderData MimeDecoderData;
typedef struct MimeEncoderData MimeEncoderData;

class nsIMimeConverter : public nsISupports {
public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMIME_CONVERTER_IID)

  // These methods are all implemented by libmime to be used by 
  // modules that need to encode/decode mail headers

  // Decode routine.
  // If header does not need decoding, places nsnull in decodedString
  NS_IMETHOD DecodeMimeHeader(const char *header, 
                              char **decodedString,
                              const char *default_charset = nsnull,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE) = 0;

  // Decode routine (also converts output to unicode)
  // On success, decodedString is never null
  NS_IMETHOD DecodeMimeHeader(const char *header, 
                              PRUnichar **decodedString,
                              const char *default_charset = nsnull,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE) = 0;

  // Decode routine (also converts output to unicode)
  // On success, decodedString is never null
  NS_IMETHOD DecodeMimeHeader(const char *header, 
                              nsAString& decodedString,
                              const char *default_charset = nsnull,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE) = 0;

  // Encode routine
  NS_IMETHOD EncodeMimePartIIStr(const char    *header, 
                                 PRBool        structured, 
                                 const char    *mailCharset, 
                                 PRInt32       fieldnamelen,
                                 const PRInt32 encodedWordSize,                                  
                                 char          **encodedString) = 0;

  // Encode routine (utf-8 input)
  NS_IMETHOD EncodeMimePartIIStr_UTF8(const char    *header, 
                                      PRBool        structured, 
                                      const char    *mailCharset, 
                                      PRInt32       fieldnamelen,
                                      const PRInt32 encodedWordSize,     
                                      char          **encodedString) = 0;

  NS_IMETHOD B64EncoderInit(nsresult (*PR_CALLBACK output_fn) (const char *buf, PRInt32 size, void *closure), 
                                void *closure, MimeEncoderData **returnEncoderData)  = 0;

  NS_IMETHOD QPEncoderInit (nsresult (*PR_CALLBACK output_fn) (const char *buf, 
                                PRInt32 size, void *closure), void *closure, 
                                MimeEncoderData ** returnEncoderData) = 0;

  NS_IMETHOD UUEncoderInit (char *filename, nsresult (*PR_CALLBACK output_fn) 
                               (const char *buf, PRInt32 size, void *closure), void *closure, 
                               MimeEncoderData ** returnEncoderData) = 0;

  NS_IMETHOD EncoderDestroy(MimeEncoderData *data, PRBool abort_p) = 0;

  NS_IMETHOD EncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size, PRInt32 *written) = 0;

}; 

#endif /* nsIMimeConverter_h_ */


