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

#include "ImportOutFile.h"
#include "nsImportTranslator.h"

#include "ImportCharSet.h"


PRBool nsImportTranslator::ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed) 
{ 
	if (pProcessed)
		*pProcessed = inLen;
	return( pOutFile->WriteData( pIn, inLen));
}

void CMHTranslator::ConvertBuffer( const PRUint8 * pIn, PRUint32 inLen, PRUint8 * pOut)
{
	while (inLen) {
		if (!ImportCharSet::IsUSAscii( *pIn) || ImportCharSet::Is822SpecialChar( *pIn) || ImportCharSet::Is822CtlChar( *pIn) ||
			(*pIn == ImportCharSet::cSpaceChar) || (*pIn == '*') || (*pIn == '\'') ||
			(*pIn == '%')) {
			// needs to be encode as %hex val
			*pOut = '%'; pOut++;
			ImportCharSet::ByteToHex( *pIn, pOut);
			pOut += 2;
		}
		else {
			*pOut = *pIn;
			pOut++;
		}
		pIn++; inLen--;
	}
	*pOut = 0;
}

PRBool CMHTranslator::ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed)
{
	PRUint8		hex[2];
	while (inLen) {
		if (!ImportCharSet::IsUSAscii( *pIn) || ImportCharSet::Is822SpecialChar( *pIn) || ImportCharSet::Is822CtlChar( *pIn) ||
			(*pIn == ImportCharSet::cSpaceChar) || (*pIn == '*') || (*pIn == '\'') ||
			(*pIn == '%')) {
			// needs to be encode as %hex val
			if (!pOutFile->WriteByte( '%'))
				return( PR_FALSE);
			ImportCharSet::ByteToHex( *pIn, hex);
			if (!pOutFile->WriteData( hex, 2))
				return( PR_FALSE);
		}
		else {
			if (!pOutFile->WriteByte( *pIn))
				return( PR_FALSE);
		}
		pIn++; inLen--;
	}

	if (pProcessed)
		*pProcessed = inLen;

	return( PR_TRUE);
}


PRBool C2047Translator::ConvertToFileQ( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed)
{
	if (!inLen)
		return( PR_TRUE);

	int		maxLineLen = 64;
	int		curLineLen = m_startLen;
	PRBool	startLine = PR_TRUE;

	PRUint8	hex[2];
	while (inLen) {
		if (startLine) {
			if (!pOutFile->WriteStr( " =?"))
				return( PR_FALSE);
			if (!pOutFile->WriteStr( m_charset.get()))
				return( PR_FALSE);
			if (!pOutFile->WriteStr( "?q?"))
				return( PR_FALSE);
			curLineLen += (6 + m_charset.Length());
			startLine = PR_FALSE;
		}

		if (!ImportCharSet::IsUSAscii( *pIn) || ImportCharSet::Is822SpecialChar( *pIn) || ImportCharSet::Is822CtlChar( *pIn) ||
			(*pIn == ImportCharSet::cSpaceChar) || (*pIn == '?') || (*pIn == '=')) {
			// needs to be encode as =hex val
			if (!pOutFile->WriteByte( '='))
				return( PR_FALSE);
			ImportCharSet::ByteToHex( *pIn, hex);
			if (!pOutFile->WriteData( hex, 2))
				return( PR_FALSE);
			curLineLen += 3;
		}
		else {
			if (!pOutFile->WriteByte( *pIn))
				return( PR_FALSE);
			curLineLen++;
		}
		pIn++; inLen--;
		if (curLineLen > maxLineLen) {
			if (!pOutFile->WriteStr( "?="))
				return( PR_FALSE);
			if (inLen) {
				if (!pOutFile->WriteStr( "\x0D\x0A "))
					return( PR_FALSE);
			}

			startLine = PR_TRUE;
			curLineLen = 0;
		}
	}

	if (!startLine) {
		// end the encoding!
		if (!pOutFile->WriteStr( "?="))
			return( PR_FALSE);
	}

	if (pProcessed)
		*pProcessed = inLen;

	return( PR_TRUE);
}

PRBool C2047Translator::ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed)
{
	if (m_useQuotedPrintable)
		return( ConvertToFileQ( pIn, inLen, pOutFile, pProcessed));

	if (!inLen)
		return( PR_TRUE);

	int			maxLineLen = 64;
	int			curLineLen = m_startLen;
	PRBool		startLine = PR_TRUE;
	int			encodeMax;
	PRUint8 *	pEncoded = new PRUint8[maxLineLen * 2];

	while (inLen) {
		if (startLine) {
			if (!pOutFile->WriteStr( " =?")) {
				delete [] pEncoded;
				return( PR_FALSE);
			}
			if (!pOutFile->WriteStr( m_charset.get())) {
				delete [] pEncoded;
				return( PR_FALSE);
			}
			if (!pOutFile->WriteStr( "?b?")) {
				delete [] pEncoded;
				return( PR_FALSE);
			}
			curLineLen += (6 + m_charset.Length());
			startLine = PR_FALSE;
		}
		encodeMax = maxLineLen - curLineLen;
		encodeMax *= 3;
		encodeMax /= 4;
		if ((PRUint32)encodeMax > inLen)
			encodeMax = (int)inLen;

		// encode the line, end the line
		// then continue. Update curLineLen, pIn, startLine, and inLen
		UMimeEncode::ConvertBuffer( pIn, encodeMax, pEncoded, maxLineLen, maxLineLen, "\x0D\x0A");

		if (!pOutFile->WriteStr( (const char *)pEncoded)) {
			delete [] pEncoded;
			return( PR_FALSE);
		}

		pIn += encodeMax;
		inLen -= encodeMax;
		startLine = PR_TRUE;
		curLineLen = 0;
		if (!pOutFile->WriteStr( "?=")) {
			delete [] pEncoded;
			return( PR_FALSE);
		}
		if (inLen) {
			if (!pOutFile->WriteStr( "\x0D\x0A ")) {
				delete [] pEncoded;
				return( PR_FALSE);
			}
		}
	}

	delete [] pEncoded;

	if (pProcessed)
		*pProcessed = inLen;

	return( PR_TRUE);
}


PRUint32	UMimeEncode::GetBufferSize( PRUint32 inBytes)
{
	// it takes 4 base64 bytes to represent 3 regular bytes
	inBytes += 3;
	inBytes /= 3;
	inBytes *= 4;
	// This should be plenty, but just to be safe
	inBytes += 4;

	// now allow for end of line characters
	inBytes += ((inBytes + 39) / 40) * 4;

	return( inBytes);
}

static PRUint8 gBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

PRUint32 UMimeEncode::ConvertBuffer( const PRUint8 * pIn, PRUint32 inLen, PRUint8 * pOut, PRUint32 maxLen, PRUint32 firstLineLen, const char * pEolStr)
{

	PRUint32	pos = 0;
	PRUint32	len = 0;
	PRUint32	lineLen = 0;
	PRUint32	maxLine = firstLineLen;
	int	eolLen = 0;
	if (pEolStr)
		eolLen = nsCRT::strlen( pEolStr);

	while ((pos + 2) < inLen) {
		// Encode 3 bytes
		*pOut = gBase64[*pIn >> 2];
		pOut++; len++; lineLen++;
		*pOut = gBase64[(((*pIn) & 0x3)<< 4) | (((*(pIn + 1)) & 0xF0) >> 4)];
		pIn++; pOut++; len++; lineLen++;
		*pOut = gBase64[(((*pIn) & 0xF) << 2) | (((*(pIn + 1)) & 0xC0) >>6)];
		pIn++; pOut++; len++; lineLen++;
		*pOut = gBase64[(*pIn) & 0x3F];
		pIn++; pOut++; len++; lineLen++;
		pos += 3;
		if (lineLen >= maxLine) {
			lineLen = 0;
			maxLine = maxLen;
			if (pEolStr) {
				nsCRT::memcpy( pOut, pEolStr, eolLen);
				pOut += eolLen;
				len += eolLen;
			}
		}
	}

	if ((pos < inLen) && ((lineLen + 3) > maxLine)) {
		lineLen = 0;
		maxLine = maxLen;
		if (pEolStr) {
			nsCRT::memcpy( pOut, pEolStr, eolLen);
			pOut += eolLen;
			len += eolLen;
		}
	}

	if (pos < inLen) {
		// Get the last few bytes!
		*pOut = gBase64[*pIn >> 2];
		pOut++; len++;
		pos++;
		if (pos < inLen) {
			*pOut = gBase64[(((*pIn) & 0x3)<< 4) | (((*(pIn + 1)) & 0xF0) >> 4)];
			pIn++; pOut++; pos++; len++;
			if (pos < inLen) {
				// Should be dead code!! (Then why is it here doofus?)
				*pOut = gBase64[(((*pIn) & 0xF) << 2) | (((*(pIn + 1)) & 0xC0) >>6)];
				pIn++; pOut++; len++;
				*pOut = gBase64[(*pIn) & 0x3F];
				pos++; pOut++; len++;
			}
			else {
				*pOut = gBase64[(((*pIn) & 0xF) << 2)];
				pOut++; len++;
				*pOut = '=';
				pOut++; len++;
			}
		}
		else {
			*pOut = gBase64[(((*pIn) & 0x3)<< 4)];
			pOut++; len++;
			*pOut = '=';
			pOut++; len++;
			*pOut = '=';
			pOut++; len++;
		}	
	}

	*pOut = 0;

	return( len);
}
