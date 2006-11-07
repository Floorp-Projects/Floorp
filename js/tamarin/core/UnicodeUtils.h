/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#ifndef __avmplus_UnicodeUtils__
#define __avmplus_UnicodeUtils__

namespace avmplus
{
	/**
	 * Utility class for converting between Unicode representations.
	 */
	class UnicodeUtils
	{
	public:
		/**
		 * Utf16ToUtf8 converts a UTF-16 sequence to UTF-8.
		 *
		 * This encoder will check UTF-16 sequences for validity,
		 * mainly meaning invalid surrogate sequences
		 * (0xD800-0xDBFF not preceding 0xDC00-0xDFFF)
		 * 
		 * Parameters:
		 * @param in      input UTF-16 buffer to encode.  The buffer is not
		 *                assumed to be zero-terminated.
		 * @param inLen   length of input buffer, in wchars.
		 * @param out     output UTF-8 buffer.  NULL may be passed, in
		 *                which case the UTF-16 sequence is checked for
		 *                validity and the number of output bytes
		 *                is calculated.
		 * @param outMax  size available in output buffer, in # of bytes.
		 *                Ignored if out is NULL.
		 *
		 * @return The number of bytes encoded, or -1 if an error occurs.
		 *
		 *    An error occurs in any of the following conditions:
		 *    - The UTF-16 sequence is invalid (bad surrogates)
		 *    - The output buffer contains insufficient space
		 *      to represent the entire UTF-8 sequence.
		 */
		static int Utf16ToUtf8(const wchar *in,
							   int inLen,
							   uint8 *out,
							   int outMax);
		
		/**
		 * Utf8ToUtf16 converts a UTF-8 sequence to UTF-16.
		 *
		 * This decoder will check UTF-8 sequences for validity,
		 * including checking for overlong sequences (considered a
		 * security risk).  It will decode UCS-4 values from
		 * 0x010000-0x10FFFF to a UTF-16 surrogate sequence.
		 * 
		 * @param in     input UTF-8 buffer to decode.  The buffer is not
		 *               assumed to be zero-terminated.
		 * @param inLen  length of input buffer, in bytes.
		 * @param out    output UTF-16 buffer.  NULL may be passed, in
		 *               which case the UTF-8 sequence is checked for
		 *               validity and the number of output characters
		 *               is calculated.
		 * @param outMax size available in output buffer, in # of wchars.
		 *               Ignored if out is NULL.
		 *
		 * @return The number of wchars decoded, or -1 if an error occurs.
		 *
		 *    An error occurs in any of the following conditions:
		 *    - The UTF-8 sequence is invalid
		 *    - The output buffer contains insufficient space
		 *      to represent the entire UTF-8 sequence.
		 *    - The UTF-8 sequence contains characters that cannot
		 *      be represented in ECMAScript (code point >0x10FFFF)
		 */
		static int Utf8ToUtf16(const uint8 *in,
							   int inLen,
							   wchar *out,
							   int outMax);

		static int Utf8Count(const uint8 *in, int inLen);

		/**
		 * Utf8ToUcs4 converts the UTF-8 sequence "chars", which
		 * is "len" bytes in length, to a single UCS-4 character.
		 * The input is not assumed to be null-terminated.
		 *
		 * The actual number of bytes consumed is returned, or
		 * 0 is the UTF-8 sequence is malformed.
		 */
		static int Utf8ToUcs4(const uint8 *chars,
							  int len,
							  uint32 *out);

		/**
		 * Ucs4ToUtf8 takes a single 32-bit UCS-4 character as
		 * input and encodes it as UTF-8.
		 *
		 * The output buffer "out" is assumed to be at least 6
		 * bytes in length.  The output is not null-terminated.
		 *
		 * Ucs4ToUtf8 returns the number of bytes written to
		 * "out", or 0 if the UCS-4 character cannot be encoded
		 * to UTF-8 (>0x7fffffff)
		 */
		static int Ucs4ToUtf8(uint32 value,
							  uint8* out);
	};
}

#endif /* __avmplus_UnicodeUtils__ */
