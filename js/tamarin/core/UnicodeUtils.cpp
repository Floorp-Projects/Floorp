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


#include "avmplus.h"

namespace avmplus
{
	/**
	 *
	 *  Table of Unicode characters that can be represented in ECMAScript
	 *  ECMA-262 Section 15.1.3
	 *
	 *	Code Point         Value Representation  1st Octet 2nd Octet 3rd Octet 4th Octet
	 *
	 *  0x0000 - 0x007F    00000000 0zzzzzzz     0zzzzzzz
	 *  0x0080 - 0x07FF    00000yyy yyzzzzzz     110yyyyy  10zzzzzz
	 *  0x0800 - 0xD7FF    xxxxyyyy yyzzzzzz     1110xxxx  10yyyyyy  10zzzzzz
	 *
	 *  0xD800 - 0xDBFF    110110vv vvwwwwxx     11110uuu  10uuwwww  10xxyyyy  10zzzzzz
	 *  followed by        followed by
	 *  0xDC00 - 0xDFFF    110111yy yyzzzzzz
	 *
	 *  0xD800 - 0xDBFF
	 *  not followed by                          causes URIError
	 *  0xDC00 - 0xDFFF
	 *
	 *  0xDC00 - 0xDFFF                          causes URIError
	 *
	 *  0xE000 - 0xFFFF    xxxxyyyy yyzzzzzz     1110xxxx 10yyyyyy 10zzzzzz
	 *
	 */

	int UnicodeUtils::Utf16ToUtf8(const wchar *in,
								  int inLen,
								  uint8 *out,
								  int outMax)
	{
		int outLen = 0;
		if (out)
		{
			// Output buffer passed in; actually encode data.
			while (inLen > 0)
			{
				wchar ch = *in;
				inLen--;
				if (ch < 0x80) {
					if (--outMax < 0) {
						return -1;
					}
					*out++ = (uint8)ch;
					outLen++;
				}
				else if (ch < 0x800) {
					if ((outMax -= 2) < 0) {
						return -1;
					}
					*out++ = (uint8)(0xC0 | (ch>>6)&0x1F);
					*out++ = (uint8)(0x80 | (ch&0x3F));
					outLen += 2;
				}
				else if (ch >= 0xD800 && ch <= 0xDBFF) {
					if (--inLen < 0) {
						return -1;
					}
					wchar ch2 = *++in;
					if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
						// This is an invalid UTF-16 surrogate pair sequence
						// Encode the replacement character instead
						ch = 0xFFFD;
						goto Encode3;
					}
					
					uint32 ucs4 = ((ch-0xD800)<<10) + (ch2-0xDC00) + 0x10000;
					if ((outMax -= 4) < 0) {
						return -1;
					}
					*out++ = (uint8)(0xF0 | (ucs4>>18)&0x07);
					*out++ = (uint8)(0x80 | (ucs4>>12)&0x3F);
					*out++ = (uint8)(0x80 | (ucs4>>6)&0x3F);
					*out++ = (uint8)(0x80 | (ucs4&0x3F));
					outLen += 4;
				}
				else {
					if (ch >= 0xDC00 && ch <= 0xDFFF) {
						// This is an invalid UTF-16 surrogate pair sequence
						// Encode the replacement character instead
						ch = 0xFFFD;
					}
				  Encode3:
					if ((outMax -= 3) < 0) {
						return -1;
					}
					*out++ = (uint8)(0xE0 | (ch>>12)&0x0F);
					*out++ = (uint8)(0x80 | (ch>>6)&0x3F);
					*out++ = (uint8)(0x80 | (ch&0x3F));
					outLen += 3;
				}
				in++;
			}
		}
		else
		{
			// Count output characters without actually encoding.
			while (inLen > 0)
			{
				wchar ch = *in;
				inLen--;
				if (ch < 0x80) {
					outLen++;
				}
				else if (ch < 0x800) {
					outLen += 2;
				}
				else if (ch >= 0xD800 && ch <= 0xDBFF) {
					if (--inLen < 0) {
						return -1;
					}
					wchar ch2 = *++in;
					if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
						// Invalid...
						// We'll encode 0xFFFD for this
						outLen += 3;
					} else {
						outLen += 4;
					}
				}
				else {
					outLen += 3;
				}
				in++;
			}
		}
		return outLen;		
	}
	
	int UnicodeUtils::Utf8ToUtf16(const uint8 *in,
								  int inLen,
								  wchar *out,
								  int outMax)
	{
		int outLen = 0;
		unsigned int outch;
		while (inLen > 0)
		{
			unsigned int c = *in;

			switch (c >> 4)
			{
			case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
			default:
				// 0xxx xxxx
				// OR not a valid utf-8 character
				// Let the converted == false case handle this.
				break;
				
			case 12: case 13:
				// 110xxxxx   10xxxxxx
				if (inLen < 2) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80) {
					// Invalid
					break;
				}
				outch = (c<<6 & 0x7C0 | in[1] & 0x3F);
				if (outch < 0x80) {
					// Overlong sequence, reject as invalid.
					break;
				}
				in += 2;
				inLen -= 2;
				if (out) {
					if (--outMax < 0) {
						return -1;
					}					
					*out++ = outch;
				}
				outLen++;
				continue;

			case 14:
				// 1110xxxx  10xxxxxx  10xxxxxx
				if (inLen < 3) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80 || (in[2]&0xC0) != 0x80) {
					// Invalid
					break;
				}
				outch = (c<<12 & 0xF000 | in[1]<<6 & 0xFC0 | in[2] & 0x3F);
				if (outch < 0x800) {
					// Overlong sequence, reject as invalid.
					break;
				}
				in += 3;
				inLen -= 3;
				if (out) {
					if (--outMax < 0) {
						return -1;
					}					
					*out++ = outch;
				}
				outLen++;
				continue;

			case 15:
				// 11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
				if (inLen < 4) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80 ||
					(in[2]&0xC0) != 0x80 ||
					(in[3]&0xC0) != 0x80)
				{
					break;
				}
				
				outch = (c<<18     & 0x1C0000 |
						in[1]<<12 & 0x3F000 &
						in[2]<<6  & 0xFC0 |
						in[3]     & 0x3F);
				if (outch < 0x10000) {
					// Overlong sequence, reject as invalid.
					break;
				}

				in += 4;
				inLen -= 4;				

				// Encode as UTF-16 surrogate sequence
				if (out) {
					if ((outMax -= 2) < 0) {
						return -1;
					}
					*out++ = (wchar) (((outch-0x10000)>>10) & 0x3FF) + 0xD800;
					*out++ = (wchar) ((outch-0x10000) & 0x3FF) + 0xDC00;
				}
				outLen += 2;
				continue;
			}

			// ! converted
			if (out) {
				if (--outMax < 0) {
					return -1;
				}
				*out++ = (wchar)c;
			}
			inLen--;
			in++;
			outLen++;
		}
		return outLen;
	}

	int UnicodeUtils::Utf8Count(const uint8 *in,
								  int inLen)
	{
		int outLen = 0;
		unsigned int outch;
		while (inLen > 0)
		{
			unsigned int c = *in;
			const uint8 *inbase = in;

			while (c <= 0x7f)
			{
				if (--inLen == 0)
					return outLen+(1+in-inbase);
				c = *(++in);
			}
			outLen += in-inbase;

			switch (c >> 4)
			{
			default:
			case 8: case 9: case 10: case 11:
				// 10xxxxxx
				// not a valid utf-8 character
				// Let the converted == false case handle this.
				break;
				
			case 12: case 13:
				// 110xxxxx   10xxxxxx
				if (inLen < 2) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80) {
					// Invalid
					break;
				}
				outch = (c<<6 & 0x7C0 | in[1] & 0x3F);
				if (outch < 0x80) {
					// Overlong sequence, reject as invalid.
					break;
				}
				in += 2;
				inLen -= 2;
				outLen++;
				continue;

			case 14:
				// 1110xxxx  10xxxxxx  10xxxxxx
				if (inLen < 3) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80 || (in[2]&0xC0) != 0x80) {
					// Invalid
					break;
				}
				outch = (c<<12 & 0xF000 | in[1]<<6 & 0xFC0 | in[2] & 0x3F);
				if (outch < 0x800) {
					// Overlong sequence, reject as invalid.
					break;
				}
				in += 3;
				inLen -= 3;
				outLen++;
				continue;

			case 15:
				// 11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
				if (inLen < 4) {
					// Invalid
					break;
				}
				if ((in[1]&0xC0) != 0x80 ||
					(in[2]&0xC0) != 0x80 ||
					(in[3]&0xC0) != 0x80)
				{
					break;
				}
				
				outch =				  (c<<18     & 0x1C0000 |
									   in[1]<<12 & 0x3F000 &
									   in[2]<<6  & 0xFC0 |
									   in[3]     & 0x3F);
				if (outch < 0x10000) {
					// Overlong sequence, reject as invalid.
					break;
				}

				in += 4;
				inLen -= 4;				

				// Encode as UTF-16 surrogate sequence
				outLen += 2;
				continue;
			}

			// not converted if we get here
			inLen--;
			in++;
			outLen++;
		}
		return outLen;
	}
								  
	int UnicodeUtils::Utf8ToUcs4(const uint8 *chars,
								 int len,
								 uint32 *out)
	{
		// U-00000000 - U-0000007F: 	0xxxxxxx
		// U-00000080 - U-000007FF: 	110xxxxx 10xxxxxx
		// U-00000800 - U-0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
		// U-00010000 - U-001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		// U-00200000 - U-03FFFFFF: 	111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		// U-04000000 - U-7FFFFFFF: 	1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

		// The minUCS4 table enforces the security rule that an
		// overlong UTF-8 sequence is forbidden, if a shorter
		// sequence could encode the same character.
		static uint32 minUCS4[] = {
			0x00000000,
			0x00000080,
			0x00000800,
			0x00010000,
			0x00200000,
			0x04000000
		};
		int n = 0;
		uint32 b;
		if (len < 1) {
			return 0;
		}
		switch (chars[0]>>4) {
		case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
			n = 1;
			b = chars[0];
			break;
		case 12: case 13:
			n = 2;
			b = chars[0]&0x1F;
			break;
		case 14:
			n = 3;
			b = chars[0]&0x0F;
			break;
		case 15:
			switch (chars[0]&0x0C) {
			case 0x00:
			case 0x04:
				n = 4;
				b = chars[0]&0x07;				
				break;
			case 0x08:
				n = 5;
				b = chars[0]&0x03;
				break;
			case 0x0C:
				n = 6;
				b = chars[0]&0x01;
				break;
			}
			// fall through intentional

		default: // invalid character, should not get here
			return 0;
		}
		if (len < n) {
			return 0;
		}
		for (int i=1; i<n; i++) {
			if ((chars[i]&0xC0) != 0x80) {
				return 0;
			}
			b = (b<<6) | chars[i]&0x3F;
		}
		if (b < minUCS4[n-1]) {
			return 0;
		}
		*out = b;
		return n;
	}

	int UnicodeUtils::Ucs4ToUtf8(uint32 value,
								 uint8 *chars)
	{
		// U-00000000 - U-0000007F: 	0xxxxxxx
		// U-00000080 - U-000007FF: 	110xxxxx 10xxxxxx
		// U-00000800 - U-0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
		// U-00010000 - U-001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		// U-00200000 - U-03FFFFFF: 	111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		// U-04000000 - U-7FFFFFFF: 	1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		if (value < 0x80) {
			*chars = (uint8)value;
			return 1;
		}
		if (value < 0x800) {
			chars[0] = (uint8)(0xC0 | (value>>6)&0x1F);
			chars[1] = (uint8)(0x80 | (value&0x3F));
			return 2;
		}
		if (value < 0x10000) {
			chars[0] = (uint8)(0xE0 | (value>>12)&0x0F);
			chars[1] = (uint8)(0x80 | (value>>6)&0x3F);
			chars[2] = (uint8)(0x80 | (value&0x3F));
			return 3;
		}
		if (value < 0x200000) {
			chars[0] = (uint8)(0xF0 | (value>>18)&0x07);
			chars[1] = (uint8)(0x80 | (value>>12)&0x3F);
			chars[2] = (uint8)(0x80 | (value>>6)&0x3F);
			chars[3] = (uint8)(0x80 | (value&0x3F));
			return 4;
		}
		if (value < 0x4000000) {
			chars[0] = (uint8)(0xF8 | (value>>24)&0x03);
			chars[1] = (uint8)(0x80 | (value>>18)&0x3F);
			chars[2] = (uint8)(0x80 | (value>>12)&0x3F);
			chars[3] = (uint8)(0x80 | (value>>6)&0x3F);			
			chars[4] = (uint8)(0x80 | (value&0x3F));
			return 5;
		}
		if (value < 0x80000000) {
			chars[0] = (uint8)(0xFC | (value>>30)&0x01);
			chars[1] = (uint8)(0x80 | (value>>24)&0x3F);			
			chars[2] = (uint8)(0x80 | (value>>18)&0x3F);
			chars[3] = (uint8)(0x80 | (value>>12)&0x3F);
			chars[4] = (uint8)(0x80 | (value>>6)&0x3F);			
			chars[5] = (uint8)(0x80 | (value&0x3F));
			return 6;
		}
		return 0;
	}
}	
