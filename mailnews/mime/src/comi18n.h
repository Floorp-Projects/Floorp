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
 */
#ifndef _COMI18N_LOADED_H_
#define _COMI18N_LOADED_H_

#include "msgCore.h"

//#ifndef kMIME_ENCODED_WORD_SIZE
//#define kMIME_ENCODED_WORD_SIZE 75
//#endif 

#ifndef kMAX_CSNAME
#define kMAX_CSNAME 64
#endif

class nsIUnicodeDecoder;
class nsIUnicodeEncoder;
class nsIStringCharsetDetector;

class MimeCharsetConverterClass {
public:
  MimeCharsetConverterClass();
  virtual ~MimeCharsetConverterClass();

  // Initialize converters for charsets, fails if converter not available.
  // 
  PRInt32 Initialize(const char* from_charset, const char* to_charset, 
                     const PRBool autoDetect=PR_FALSE, const PRInt32 maxNumCharsDetect=-1);

  // Converts input buffer or duplicates input if converters not available (and returns 0).
  // Also duplicates input if convertion not needed.
  // C string is generated for converted string.
  PRInt32 Convert(const char* inBuffer, const PRInt32 inLength, 
                  char** outBuffer, PRInt32* outLength,
                  PRInt32* numUnConverted);

  static nsIStringCharsetDetector *mDetector;  // charset detector

protected:
  nsIUnicodeDecoder * GetUnicodeDecoder() {return (mAutoDetect && NULL != mDecoderDetected) ? mDecoderDetected : mDecoder;}
  nsIUnicodeEncoder * GetUnicodeEncoder() {return mEncoder;}
  PRBool NeedCharsetConversion(const nsString& from_charset, const nsString& to_charset);

private:
  nsIUnicodeDecoder *mDecoder;          // decoder (convert to unicode)  
  nsIUnicodeEncoder *mEncoder;          // encoder (convert from unicode)
  nsIUnicodeDecoder *mDecoderDetected;  // decoder of detected charset (after when auto detection succeeded)
  PRInt32 mMaxNumCharsDetect;           // maximum number of characters in bytes to abort auto detection 
                                        // (-1 for no limit)
  PRInt32 mNumChars;                    // accumulated number of characters converted in bytes
  PRBool mAutoDetect;                   // true if apply auto detection
  nsString mInputCharset;               // input charset for auto detection hint as well as need conversion check
  nsString mOutputCharset;              // output charset for need conversion check
  static nsCString mDetectorContractID;     // ContractID of charset detector
};

  

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * If a header is MIME encoded then decode a header and sets a charset name.
 * This is a replacement for INTL_DecodeMimePartIIStr.
 * Unlike INTL_DecodeMimePartIIStr, this does not apply any charset conversion.
 * Use MIME_ConvertCharset if the decoded string needs a conversion.
 *
 *
 * @param header      [IN] A header to decode.
 * @param charset     [OUT] Charset name (in C string) from a MIME header is set.
 *                    Caller should allocate at least 65 bytes (kMAX_CSNAME + 1) for a charset name.
 * @return            Decoded buffer (in C string) or return NULL if the header is not MIME encoded.
 */
extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset,
										  PRBool eatContinuations);

/**
 * Encode an input string into RFC 2047 form.
 * This is a replacement for INTL_EncodeMimePartIIStr.
 * Unlike INTL_EncodeMimePartIIStr, this does not apply any charset conversion.
 * Use MIME_ConvertCharset in advance if the encoding string needs a conversion.
 *
 *
 * @param header          [IN] A header to encode (utf-8 Cstring).
 * @param mailCharset     [IN] Charset name (in C string) to convert.
 * @param encodedWordSize [IN] Byte length limit of the output, ususally 72 (use kMIME_ENCODED_WORD_SIZE).
 * @return            Encoded buffer (in C string) or NULL in case of error.
 */
char *MIME_EncodeMimePartIIStr(const char *header, const char* mailCharset, const PRInt32 encodedWordSize);

/**
 * Apply charset conversion to a given string. The conversion is done by an unicode round trip.
 * This is a replacement for INTL_ConvertLineWithoutAutoDetect.
 * The caller should instanticate converters by XPCOM for that purpose.
 *
 * Note the caller cannot call this muliple times for a large buffer (of multi byte text)
 * since this will not save a state info (i.e. converter instance will be created/destroyed for every call).
 *
 * @param from_charset[IN] A charset name in C string.
 * @param to_charset  [IN] A charset name in C string.
 * @param inCstring   [IN] Input buffer (in C string) to convert.
 * @param outCstring  [OUT] Converted buffer (in C string) is set. Allocated buffer should be freed by PR_FREE.
 * @return            0 is success, otherwise error.
 */
PRInt32 MIME_ConvertString(const char* from_charset, const char* to_charset,
                             const char* inCstring, char** outCstring);

/**
 * Apply charset conversion to a given buffer. The conversion is done by an unicode round trip.
 *
 * Note the caller cannot call this muliple times for a large buffer (of multi byte text)
 * since this will not save a state info (i.e. converter instance will be created/destroyed for every call).
 * The caller should instanticate converters by XPCOM for that purpose.
 *
 * @param autoDetection [IN] True if apply auto charset detection.
 * @param from_charset[IN] A charset name in C string.
 * @param to_charset  [IN] A charset name in C string.
 * @param inBuffer    [IN] Input buffer to convert.
 * @param inLength    [IN] Input buffer length.
 * @param outBuffer   [OUT] Converted buffer is set. Allocated buffer should be freed by PR_FREE.
 * @param outLength   [OUT] Converted buffer length is set.
 * @param numUnConverted [OUT] Number of unconverted characters (can be NULL).
 * @return            0 is success, otherwise error.
 */
PRInt32 MIME_ConvertCharset(const PRBool autoDetection, const char* from_charset, const char* to_charset,
                            const char* inBuffer, const PRInt32 inLength, char** outBuffer, PRInt32* outLength,
                            PRInt32* numUnConverted);


/**
 * Get a next character position in an UTF-8 string.
 * Example: s = NextChar_UTF8(s);  // get a pointer for the next character
 *
 *
 * @param str          [IN] An input C string (UTF-8).
 * @return             A pointer to the next character.
 */
char * NextChar_UTF8(char *str);

/*
 * To be removed. Existing for the backword compatibility. 
 */
char *INTL_DecodeMimePartIIStr(const char *header, PRInt16 wincsid, PRBool dontConvert);
char *INTL_EncodeMimePartIIStr(char *subject, PRInt16 wincsid, PRBool bUseMime);
char *INTL_EncodeMimePartIIStr_VarLen(char *subject, PRInt16 wincsid, PRBool bUseMime, int encodedWordSize);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // _COMI18N_LOADED_H_

