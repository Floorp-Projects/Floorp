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


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef kMIME_ENCODED_WORD_SIZE
#define kMIME_ENCODED_WORD_SIZE 75
#endif 

#ifndef kMAX_CSNAME
#define kMAX_CSNAME 64
#endif

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
extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset);

/**
 * Encode an input string into RFC 2047 form.
 * This is a replacement for INTL_EncodeMimePartIIStr.
 * Unlike INTL_EncodeMimePartIIStr, this does not apply any charset conversion.
 * Use MIME_ConvertCharset in advance if the encoding string needs a conversion.
 *
 *
 * @param header          [IN] A header to encode (utf-8 Cstring).
 * @param mailCharset     [IN] Charset name (in C string) to convert.
 * @param encodedWordSize [IN] Byte lenght limit of the output, ususally 72 (use kMIME_ENCODED_WORD_SIZE).
 * @return            Encoded buffer (in C string) or NULL in case of error.
 */
char *MIME_EncodeMimePartIIStr(const char *header, const char* mailCharset, const PRInt32 encodedWordSize);

/**
 * Apply charset conversion to a given buffer.
 * This is a replacement for INTL_ConvertLineWithoutAutoDetect.
 *
 *
 * @param from_charset[IN] A charset name in C string.
 * @param to_charset  [IN] A charset name in C string.
 * @param inCstring   [IN] Input buffer (in C string) to convert.
 * @param outCstring  [OUT] Converted buffer (in C string) is set. Allocated buffer should be freed by PR_FREE.
 * @return            nsresult, 0 is success, otherwise error.
 */
PRUint32 MIME_ConvertCharset(const char* from_charset, const char* to_charset,
                             const char* inCstring, char** outCstring);

/**
 * Convert an input buffer with a charset into unicode.
 *
 *
 * @param from_charset [IN] A charset name in C string.
 * @param inCstring    [IN] Input buffer (in C string) to convert.
 * @param uniBuffer    [OUT] Output unicode buffer is set. Allocated buffer should be freed by PR_FREE.
 * @param uniLength    [OUT] Output unicode buffer character length is set.
 * @return             nsresult, 0 is success, otherwise error.
 */
PRUint32 MIME_ConvertToUnicode(const char* from_charset, const char* inCstring,
                               void** uniBuffer, PRInt32* uniLength);

/**
 * Conver an unicode buffer to a given charset.
 *
 *
 * @param to_charset   [IN] A charset name in C string.
 * @param uniBuffer    [IN] Input unicode buffer to convert.
 * @param uniLength    [IN] Input unicode buffer character length.
 * @param outCstring   [OUT] Output buffer (in C string) is set. Allocated buffer should be freed by PR_FREE.
 * @return             nsresult, 0 is success, otherwise error.
 */
PRUint32 MIME_ConvertFromUnicode(const char* to_charset, const void* uniBuffer, const PRInt32 uniLength,
                                 char** outCstring);


/**
 * Get a next character position in an UTF-8 string.
 * Example: s += NextChar_UTF8(s);  // advance a pointer for one character
 *
 *
 * @param str          [IN] An input C string (UTF-8).
 * @return             A pointer to the next character.
 */
unsigned char * NextChar_UTF8(unsigned char *str);

/*
 * To be removed. Existing for the backword compatibility. 
 */
char *INTL_DecodeMimePartIIStr(const char *header, int16 wincsid, PRBool dontConvert);
char *INTL_EncodeMimePartIIStr(char *subject, int16 wincsid,PRBool bUseMime);
char *INTL_EncodeMimePartIIStr_VarLen(char *subject, int16 wincsid, PRBool bUseMime, int encodedWordSize);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
