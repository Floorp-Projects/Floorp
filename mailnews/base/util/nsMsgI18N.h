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
 * @param header       [IN] A header to encode.
 * @param structured   [IN] Specify the header is structured or non-structured field (See RFC-822).
 * @param charset      [IN] Charset name to convert.
 * @param fieldnamelen [IN] Header field name length. (e.g. "From: " -> 6)
 * @param usemime      [IN] If false then apply charset conversion only no MIME encoding.
 * @return             Encoded buffer (in C string) or NULL in case of error.
 */
NS_MSG_BASE char      *nsMsgI18NEncodeMimePartIIStr(const char *header, PRBool structured, const char *charset, PRInt32 fieldnamelen, PRBool usemime);

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
 * Check the input (unicode) string is in a range of the given charset after the conversion.
 * Note, do not use this for large string (e.g. message body) since this actually applies the conversion to the buffer.
 *
 * @param charset     [IN] Charset to be converted.
 * @param inString    [IN] Input unicode string to be examined.
 * @param fallbackCharset [OUT]
 *                         null if fallback charset is not needed.
 *                         Otherwise, a fallback charset name may be set if that was used for the conversion. 
 *                         Caller is responsible for freeing the memory (or use nsXPIDLCString).
 * @return            True if the string can be converted within the charset range.
 *                    False if one or more characters cannot be converted to the target charset.
 */
NS_MSG_BASE PRBool    nsMsgI18Ncheck_data_in_charset_range(const char *charset, const PRUnichar* inString,
                                                           char **fallbackCharset=nsnull);

/**
 * Return accept language.
 * Note, the return value is not valid after this method gets called again.
 *
 * @return            Accept language.
 */
NS_MSG_BASE const char *nsMsgI18NGetAcceptLanguage(void); 

/**
 * Return charset name of file system (OS dependent).
 *
 * @return            File system charset name.
 */
NS_MSG_BASE const char * nsMsgI18NFileSystemCharset(void);

/**
 * Convert from unicode to target charset.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Unicode string to convert.
 * @param outString   [OUT] Converted output string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertFromUnicode(const char* aCharset,
                                                 const nsAFlatString& inString,
                                                 nsACString& outString,
                                                 PRBool aIsCharsetCanonical =
                                                        PR_FALSE);
/**
 * Convert from charset to unicode.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Input string to convert.
 * @param outString   [OUT] Output unicode string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertToUnicode(const char* aCharset,
                                               const nsAFlatCString& inString,
                                               nsAString& outString,
                                               PRBool aIsCharsetCanonical =
                                                      PR_FALSE);
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
 * @param fallbackCharset [OUT]
 *                         null if fallback charset is not needed.
 *                         Otherwise, a fallback charset name may be set if that was used for the conversion. 
 *                         Caller is responsible for freeing the memory (or use nsXPIDLCString).
 * @param isAsciiOnly [OUT]
 *                         null if non ASCII info is not needed.
 *                         Otherwise, true is set if the input data is ASCII only false otherwise. 
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NSaveAsCharset(const char* contentType, const char* charset, 
                                            const PRUnichar* inString, char** outString, 
                                            char **fallbackCharset=nsnull, PRBool *isAsciiOnly=nsnull);

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

// inline forwarders to avoid littering with 'x-imap4-.....'
inline nsresult CopyUTF16toMUTF7(const nsAFlatString &aSrc, nsACString& aDest)
{
    return nsMsgI18NConvertFromUnicode("x-imap4-modified-utf7", aSrc,
                                       aDest, PR_TRUE);
}

inline nsresult CopyMUTF7toUTF16(const nsAFlatCString& aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode("x-imap4-modified-utf7", aSrc,
                                     aDest, PR_TRUE);
}

inline nsresult ConvertToUnicode(const char* charset,
                                 const nsAFlatCString &aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode(charset, aSrc, aDest);
}

inline nsresult ConvertToUnicode(const char* charset,
                                 const char* aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode(charset, nsDependentCString(aSrc), aDest);
}

inline nsresult ConvertFromUnicode(const char* charset,
                                   const nsAFlatString &aSrc, nsACString& aDest)
{
    return nsMsgI18NConvertFromUnicode(charset, aSrc, aDest);
}

// XXX to be replaced by NS_CopyNativeToUnicode
inline nsresult nsMsgI18NCopyNativeToUTF16(const nsAFlatCString &aSrc,
                                           nsAString &aDest)
{
  return nsMsgI18NConvertToUnicode(nsMsgI18NFileSystemCharset(), aSrc, aDest);
}

inline nsresult nsMsgI18NCopyNativeToUTF16(const char* aSrc, nsAString &aDest)
{
  return nsMsgI18NCopyNativeToUTF16(nsDependentCString(aSrc), aDest);
}

// XXX to be replaced by NS_CopyUnicodeToNative
inline nsresult nsMsgI18NCopyUTF16ToNative(const nsAFlatString &aSrc,
                                           nsACString &aDest)
{
  return nsMsgI18NConvertFromUnicode(nsMsgI18NFileSystemCharset(), aSrc, aDest);
}
inline nsresult nsMsgI18NCopyUTF16ToNative(const PRUnichar* aSrc,
                                           nsACString &aDest)
{
  return nsMsgI18NCopyUTF16ToNative(nsDependentString(aSrc), aDest);
}

#endif /* _nsMsgI18N_H_ */
