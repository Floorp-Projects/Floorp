/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
/*
 * This interface allows any module to access the encoder/decoder 
 * routines for RFC822 headers, base64 and QP encoders. This will 
 * allow any mail/news module to call on these routines.
 */
#ifndef nsIMimeConverter_h_
#define nsIMimeConverter_h_

#include "prtypes.h"
#include "nsString.h"

// {866A1E11-D0B9-11d2-B373-525400E2D63A}
#define NS_MIME_CONVERTER_CID   \
        { 0x866a1e11, 0xd0b9, 0x11d2,         \
        { 0xb3, 0x73, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

// {C09EDB23-B7AF-11d2-B35E-525400E2D63A}
#define NS_IMIME_CONVERTER_IID \
      { 0xfb953da, 0xd0b5, 0x11d2,   \
      { 0xb3, 0x73, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

// default line length for calling the encoder
#define kMIME_ENCODED_WORD_SIZE       72 

// Max length of charset name. 
#define kMAX_CSNAME                   64 

/* Opaque objects used by the encoder/decoder to store state. */
typedef struct MimeDecoderData MimeDecoderData;
typedef struct MimeEncoderData MimeEncoderData;

class nsIMimeConverter : public nsISupports {
public: 
  static const nsIID& GetIID() { static nsIID iid = NS_IMIME_CONVERTER_IID; return iid; }

  // These methods are all implemented by libmime to be used by 
  // modules that need to encode/decode mail headers

  // Decode routine
  NS_IMETHOD DecodeMimePartIIStr(const char *header, 
                                 char       *charset, 
                                 char **decodedString) = 0;

  // Decode routine (also converts output to unicode)
  NS_IMETHOD DecodeMimePartIIStr(const nsString& header, 
                                 nsString& charset, 
                                 nsString& decodedString) = 0;

  // Encode routine
  NS_IMETHOD EncodeMimePartIIStr(const char    *header, 
                                 const char    *mailCharset, 
                                 const PRInt32 encodedWordSize,                                  
                                 char          **encodedString) = 0;

  NS_IMETHOD B64EncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), 
                                void *closure, MimeEncoderData **returnEncoderData)  = 0;

  NS_IMETHOD QPEncoderInit (int (*output_fn) (const char *buf, 
                                PRInt32 size, void *closure), void *closure, 
                                MimeEncoderData ** returnEncoderData) = 0;

  NS_IMETHOD UUEncoderInit (char *filename, int (*output_fn) 
                               (const char *buf, PRInt32 size, void *closure), void *closure, 
                               MimeEncoderData ** returnEncoderData) = 0;

  NS_IMETHOD EncoderDestroy(MimeEncoderData *data, PRBool abort_p) = 0;

  NS_IMETHOD EncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size, PRInt32 *written) = 0;

}; 

#endif /* nsIMimeConverter_h_ */


