/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _nsMsgI18N_H_
#define _nsMsgI18N_H_

#include "nscore.h"
#include "msgCore.h"
class nsFileSpec;

#ifndef kMAX_CSNAME
#define kMAX_CSNAME 64
#endif

/**
 * Encode an input string into RFC 2047 form.
 *
 * @param header      [IN] A header to encode.
 * @param charset     [IN] Charset name to convert.
 * @param bUseMime    [IN] If false then apply charset conversion only no MIME encoding.
 * @return            Encoded buffer (in C string) or NULL in case of error.
 */
NS_MSG_BASE char      *nsMsgI18NEncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime);

/**
 * Check if given charset is stateful (e.g. ISO-2022-JP).
 *
 * @param charset     [IN] Charset name.
 * @return            True if stateful
 */
NS_MSG_BASE PRBool    nsMsgI18Nstateful_charset(const char *charset);

/**
 * Check if given charset is multibye (e.g. Shift_JIS, Big5).
 *
 * @param charset     [IN] Charset name.
 * @return            True if multibyte
 */
NS_MSG_BASE PRBool nsMsgI18Nmultibyte_charset(const char *charset);

/**
 * Check the input string contains 7 bit data only.
 *
 * @param charset     [IN] Charset name of the input string.
 * @param string      [IN] Input string to be examined.
 * @param size        [IN] Size of the input string.
 * @return            True if 7 bit only.
 */
NS_MSG_BASE PRBool    nsMsgI18N7bit_data_part(const char *charset, const char *string, const PRUint32 size);

/**
 * Check the input (unicode) string is in a range of the given charset after the conversion.
 * Note, do not use this for large string (e.g. message body) since this actually applies the conversion to the buffer.
 *
 * @param charset     [IN] Charset to be converted.
 * @param inString    [IN] Input unicode string to be examined.
 * @return            True if the string can be converted within the charset range.
 *                    False if one or more characters cannot be converted to the target charset.
 */
NS_MSG_BASE PRBool    nsMsgI18Ncheck_data_in_charset_range(const char *charset, const PRUnichar* inString);

/**
 * Return accept language.
 *
 * @return            Accept language.
 */
NS_MSG_BASE char      *nsMsgI18NGetAcceptLanguage(void); 

/**
 * Return charset name internally used in messsage compose.
 *
 * @return            Charset name.
 */
NS_MSG_BASE const char *msgCompHeaderInternalCharset(void);

/**
 * Return charset name of file system (OS dependent).
 *
 * @return            File system charset name.
 */
NS_MSG_BASE const nsString& nsMsgI18NFileSystemCharset(void);

/**
 * Return mail/news default send charset.
 *
 * @return            Mail/news default charset for mail send.
 */
NS_MSG_BASE char * nsMsgI18NGetDefaultMailCharset(void);

/**
 * Convert from unicode to target charset.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Unicode string to convert.
 * @param outString   [OUT] Converted output string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertFromUnicode(const nsCString& aCharset, 
                                     const nsString& inString,
                                     nsCString& outString);

/**
 * Convert from charset to unicode.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Input string to convert.
 * @param outString   [OUT] Output unicode string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertToUnicode(const nsCString& aCharset, 
                                   const nsCString& inString, 
                                   nsString& outString);

/**
 * Convert from unicode to target charset.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Unicode string to convert.
 * @param outCString  [OUT] Output C string, need PR_FREE.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult ConvertFromUnicode(const nsString& aCharset, 
                                   const nsString& inString,
                                   char** outCString);

/**
 * Convert from charset to unicode.
 *
 * @param charset     [IN] Charset name.
 * @param inCString   [IN] C string to convert.
 * @param outString   [OUT] Output unicode string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult ConvertToUnicode(const nsString& aCharset, 
                                 const char* inCString, 
                                 nsString& outString);

/**
 * Parse for META charset.
 *
 * @param fileSpec    [IN] A filespec.
 * @return            A charset name or empty string if not found.
 */
NS_MSG_BASE const char *nsMsgI18NParseMetaCharset(nsFileSpec* fileSpec);

/**
 * Convert input to HTML entities (e.g. &nbsp;, &aacute;).
 *
 * @param inString    [IN] Input string to convert.
 * @param outString   [OUT] Converted output string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertToEntity(const nsString& inString, nsString* outString);

/**
 * Convert from charset to unicode. Also does substitution for unconverted characters (e.g. entity, '?').
 *
 * @param contentType [IN] text/plain or text/html.
 * @param charset     [IN] Charset name to convert.
 * @param inString    [IN] Input unicode string to convert.
 * @param outString   [OUT] Allocated and converted output C string. Need PR_FREE.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NSaveAsCharset(const char* contentType, const char* charset, const PRUnichar* inString, char** outString);

/**
 * Convert from unicode to charset, generates NNTP XPAT search string.
 * Migrated from mozilla classic, 
 * see "http://lxr.mozilla.org/classic/source/include/libi18n.h#2162" for detail.
 *
 * @param aCharset    [IN] Charset name to convert.
 * @param inString    [IN] Input unicode string to convert.
 * @param outString   [OUT] Converted output, the pattern should be send to NNTP XPAT command 
 *                          for searching non-ASCII header.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NFormatNNTPXPATInNonRFC1522Format(const nsCString& aCharset, const nsString& inString, nsCString& outString);

#endif /* _nsMsgI18N_H_ */
