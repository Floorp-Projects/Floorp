/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef ImportCharSet_h___
#define ImportCharSet_h___

#include "nscore.h"


// Some useful ASCII values
//	'A' = 65, 0x41
// 	'Z' = 90, 0x5a
//	'_' = 95, 0x5f
//	'a' = 97, 0x61
//	'z' = 122, 0x7a
//	'0' = 48, 0x30
//	'1' = 49, 0x31
//	'9' = 57, 0x39
//	' ' = 32, 0x20
// 	whitespace, 10, 13, 32, 9 (linefeed, cr, space, tab) - 0x0a, 0x0d, 0x20, 0x09
//	':' = 58, 0x3a


// a typedef enum would be nicer but some compilers still have trouble with treating
// enum's as plain numbers when needed

class ImportCharSet {
public:
	enum {
		cTabChar = 9,
		cLinefeedChar = 10,
		cCRChar = 13,
		cSpaceChar = 32,
		cUpperAChar = 65,
		cUpperZChar = 90,
		cUnderscoreChar = 95,
		cLowerAChar = 97,
		cLowerZChar = 122,
		cZeroChar = 48,
		cNineChar = 57,

		cAlphaNumChar = 1,
		cAlphaChar = 2,
		cWhiteSpaceChar = 4,
		cDigitChar = 8,
		c822SpecialChar = 16
	};

	static char			m_upperCaseMap[256];
	static char			m_Ascii[256];

	inline static PRBool IsUSAscii( PRUint8 ch) { return( ((ch & (PRUint8)0x80) == 0));}
	inline static PRBool Is822CtlChar( PRUint8 ch) { return( (ch < 32));}
	inline static PRBool Is822SpecialChar( PRUint8 ch) { return( (m_Ascii[ch] & c822SpecialChar) == c822SpecialChar);}
	inline static PRBool IsWhiteSpace( PRUint8 ch) { return( (m_Ascii[ch] & cWhiteSpaceChar) == cWhiteSpaceChar); }
	inline static PRBool IsAlphaNum( PRUint8 ch) { return( (m_Ascii[ch] & cAlphaNumChar) == cAlphaNumChar); }
	inline static PRBool IsDigit( PRUint8 ch) { return( (m_Ascii[ch] & cDigitChar) == cDigitChar); }

	inline static PRUint8 ToLower( PRUint8 ch) { if ((m_Ascii[ch] & cAlphaChar) == cAlphaChar) { return( cLowerAChar + (m_upperCaseMap[ch] - cUpperAChar)); } else return( ch); }

	inline static long AsciiToLong( const PRUint8 * pChar, PRUint32 len) {
		long num = 0;
		while (len) {
			if ((m_Ascii[*pChar] & cDigitChar) == 0)
				return( num);
			num *= 10;
			num += (*pChar - cZeroChar);
			len--;
			pChar++;
		}
		return( num);
	}

	inline static void ByteToHex( PRUint8 byte, PRUint8 * pHex) {
		PRUint8 val = byte;
		val /= 16;
		if (val < 10)
			*pHex = '0' + val;
		else
			*pHex = 'A' + (val - 10);
		pHex++;
		val = byte;
		val &= 0x0F;
		if (val < 10)
			*pHex = '0' + val;
		else
			*pHex = 'A' + (val - 10);		
	}

	inline static void	LongToHexBytes( PRUint32 type, PRUint8 * pStr) {
		ByteToHex( (PRUint8) (type >> 24), pStr);
		pStr += 2;
		ByteToHex( (PRUint8) ((type >> 16) & 0x0FF), pStr);
		pStr += 2;
		ByteToHex( (PRUint8) ((type >> 8) & 0x0FF), pStr);
		pStr += 2;
		ByteToHex( (PRUint8) (type & 0x0FF), pStr);
	}

	inline static void SkipWhiteSpace( const PRUint8 * & pChar, PRUint32 & pos, PRUint32 max) {
		while ((pos < max) && (IsWhiteSpace( *pChar))) {
			pos++; pChar++;
		}
	}

	inline static void SkipSpaceTab( const PRUint8 * & pChar, PRUint32& pos, PRUint32 max) {
		while ((pos < max) && ((*pChar == (PRUint8)cSpaceChar) || (*pChar == (PRUint8)cTabChar))) {
			pos++; pChar++;
		}
	}

	inline static void SkipTilSpaceTab( const PRUint8 * & pChar, PRUint32& pos, PRUint32 max) {
		while ((pos < max) && (*pChar != (PRUint8)cSpaceChar) && (*pChar != (PRUint8)cTabChar)) {
			pos++;
			pChar++;
		}
	}

	inline static PRBool StrNICmp( const PRUint8 * pChar, const PRUint8 * pSrc, PRUint32 len) {
		while (len && (m_upperCaseMap[*pChar] == m_upperCaseMap[*pSrc])) {
			pChar++; pSrc++; len--;
		}
		return( len == 0);
	}

	inline static PRBool StrNCmp( const PRUint8 * pChar, const PRUint8 *pSrc, PRUint32 len) {
		while (len && (*pChar == *pSrc)) {
			pChar++; pSrc++; len--;
		}
		return( len == 0);
	}

	inline static int FindChar( const PRUint8 * pChar, PRUint8 ch, PRUint32 max) {
		PRUint32		pos = 0;
		while ((pos < max) && (*pChar != ch)) {
			pos++; pChar++;
		}
		if (pos < max)
			return( (int) pos);
		else
			return( -1);
	}

	inline static PRBool NextChar( const PRUint8 * & pChar, PRUint8 ch, PRUint32& pos, PRUint32 max) {
		if ((pos < max) && (*pChar == ch)) {
			pos++;
			pChar++;
			return( PR_TRUE);
		}
		return( PR_FALSE);
	}

	inline static PRInt32 strcmp( const char * pS1, const char * pS2) {
		while (*pS1 && *pS2 && (*pS1 == *pS2)) {
			pS1++;
			pS2++;
		}
		return( *pS1 - *pS2);
	}

	inline static PRInt32 stricmp( const char * pS1, const char * pS2) {
		while (*pS1 && *pS2 && (m_upperCaseMap[*pS1] == m_upperCaseMap[*pS2])) {
			pS1++;
			pS2++;
		}
		return( m_upperCaseMap[*pS1] - m_upperCaseMap[*pS2]);
	}

};


#endif /* ImportCharSet_h__ */

