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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nscore.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsImportMimeEncode.h"

#include "ImportCharSet.h"
#include "ImportTranslate.h"

#define	kNoState		0
#define	kStartState		1
#define	kEncodeState	2
#define kDoneState		3

#define kEncodeBufferSz	(8192 * 8)

nsImportMimeEncode::nsImportMimeEncode()
{
	m_pOut = nsnull;
	m_state = kNoState;
	m_bytesProcessed = 0;
	m_pInputBuf = nsnull;
	m_pMimeFile = nsnull;
}

nsImportMimeEncode::~nsImportMimeEncode()
{
	NS_IF_RELEASE( m_pMimeFile);
	if (m_pInputBuf)
		delete [] m_pInputBuf;
}

void nsImportMimeEncode::EncodeFile( nsIFileSpec *pInFile, ImportOutFile *pOut, const char *pFileName, const char *pMimeType)
{
	m_fileName = pFileName;
	m_mimeType = pMimeType;

	m_pMimeFile = pInFile;
	NS_IF_ADDREF( m_pMimeFile);

	m_pOut = pOut;
	m_state = kStartState;
}

void nsImportMimeEncode::CleanUp( void)
{
	CleanUpEncodeScan();
}

PRBool nsImportMimeEncode::SetUpEncode( void)
{
	nsCString		errStr;
	if (!m_pInputBuf) {
		m_pInputBuf = new PRUint8[kEncodeBufferSz];
	}
	
	m_appleSingle = PR_FALSE;

#ifdef _MAC_IMPORT_CODE
	// First let's see just what kind of beast we have?
	// For files with only a data fork and a known mime type
	// proceed with normal mime encoding just as on the PC.
	// For unknown mime types and files with both forks,
	// encode as AppleSingle format.
	if (m_filePath.GetMacFileSize( UFileLocation::eResourceFork) || !pMimeType) {
		m_appleSingle = TRUE;
		m_mimeType = "application/applefile";
	}
#endif
	
	if (!InitEncodeScan( m_appleSingle, m_pMimeFile, m_fileName, m_pInputBuf, kEncodeBufferSz)) {
		return( PR_FALSE);
	}
		
	m_state = kEncodeState;
	m_lineLen = 0;
	
	// Write out the boundary header
	PRBool bResult = PR_TRUE;
	bResult = m_pOut->WriteStr( "Content-type: ");
	if (bResult)
		bResult = m_pOut->WriteStr( m_mimeType);

#ifdef _MAC_IMPORT_CODE
	// include the type an creator here
	if (bResult)
		bResult = m_pOut->WriteStr( "; x-mac-type=\"");
	U8	hex[8];
	LongToHexBytes( m_filePath.GetFileType(), hex);
	if (bResult)
		bResult = m_pOut->WriteData( hex, 8);
	LongToHexBytes( m_filePath.GetFileCreator(), hex);
	if (bResult)
		bResult = m_pOut->WriteStr( "\"; x-mac-creator=\"");
	if (bResult)
		bResult = m_pOut->WriteData( hex, 8);
	if (bResult)
		bResult = m_pOut->WriteStr( "\"");
#endif

	/*
	if (bResult)
		bResult = m_pOut->WriteStr( gMimeTypeFileName);
	*/
	if (bResult)
		bResult = m_pOut->WriteStr( ";\x0D\x0A");

	nsCString		fName;
	PRBool			trans = TranslateFileName( m_fileName, fName);
	if (bResult)
		bResult = WriteFileName( fName, trans, "name");
	if (bResult)
		bResult = m_pOut->WriteStr( "Content-transfer-encoding: base64");
	if (bResult)
		bResult = m_pOut->WriteEol();
	if (bResult)
		bResult = m_pOut->WriteStr( "Content-Disposition: attachment;\x0D\x0A");
	if (bResult)
		bResult = WriteFileName( fName, trans, "filename");
	if (bResult)
		bResult = m_pOut->WriteEol();

	if (!bResult) {
		CleanUp();
	}

	return( bResult);
}

PRBool nsImportMimeEncode::DoWork( PRBool *pDone)
{	
	*pDone = PR_FALSE;
	switch( m_state) {
	case kNoState:
		return( PR_FALSE);
		break;
	case kStartState:
		return( SetUpEncode());
		break;
	case kEncodeState:
		if (!Scan( pDone)) {
			CleanUp();
			return( PR_FALSE);
		}
		if (*pDone) {
			*pDone = PR_FALSE;
			m_state = kDoneState;
		}
		break;
	case kDoneState:
		CleanUp();
		m_state = kNoState;
		*pDone = PR_TRUE;
		break;
	}

	return( PR_TRUE);
}

static PRUint8 gBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

PRBool nsImportMimeEncode::ScanBuffer( PRBool *pDone)
{

	PRUint32	pos = m_pos;
	PRUint32	start = pos;
	PRUint8 *	pChar = m_pBuf + pos;
	PRUint32	max = m_bytesInBuf;
	PRUint8		byte[4];
	PRUint32	lineLen = m_lineLen;

	while ((pos + 2) < max) {
		// Encode 3 bytes
		byte[0] = gBase64[*pChar >> 2];
		byte[1] = gBase64[(((*pChar) & 0x3)<< 4) | (((*(pChar + 1)) & 0xF0) >> 4)];
		pChar++;
		byte[2] = gBase64[(((*pChar) & 0xF) << 2) | (((*(pChar + 1)) & 0xC0) >>6)];
		pChar++;
		byte[3] = gBase64[(*pChar) & 0x3F];
		if (!m_pOut->WriteData( byte, 4))
			return( PR_FALSE);
		pos += 3;
		pChar++;
		lineLen += 4;
		if (lineLen > 71) {
			if (!m_pOut->WriteEol())
				return( PR_FALSE);
			lineLen = 0;
		}
	}

	if ((pos < max) && m_eof) {
		// Get the last few bytes!
		byte[0] = gBase64[*pChar >> 2];
		pos++;
		if (pos < max) {
			byte[1] = gBase64[(((*pChar) & 0x3)<< 4) | (((*(pChar + 1)) & 0xF0) >> 4)];
			pChar++;
			pos++;
			if (pos < max) {
				// Should be dead code!! (Then why is it here doofus?)
				byte[2] = gBase64[(((*pChar) & 0xF) << 2) | (((*(pChar + 1)) & 0xC0) >>6)];
				pChar++;
				byte[3] = gBase64[(*pChar) & 0x3F];
				pos++;
			}
			else {
				byte[2] = gBase64[(((*pChar) & 0xF) << 2)];
				byte[3] = '=';
			}
		}
		else {
			byte[1] = gBase64[(((*pChar) & 0x3)<< 4)];
			byte[2] = '=';
			byte[3] = '=';
		}
		
		if (!m_pOut->WriteData( byte, 4))
			return( PR_FALSE);
		if (!m_pOut->WriteEol())
			return( PR_FALSE);
	}
	else if (m_eof) {
		/*
		byte[0] = '=';
		if (!m_pOut->WriteData( byte, 1))
			return( FALSE);
		*/
		if (!m_pOut->WriteEol())
			return( PR_FALSE);
	}
	
	m_lineLen = (int) lineLen;
	m_pos = pos;
	m_bytesProcessed += (pos - start);
	return( PR_TRUE);
}

PRBool nsImportMimeEncode::TranslateFileName( nsCString& inFile, nsCString& outFile)
{
	const PRUint8 * pIn = (const PRUint8 *) (const char *)inFile;
	int	  len = inFile.Length();
	
	while (len) {
		if (!ImportCharSet::IsUSAscii( *pIn))
			break;
		len--;
		pIn++;
	}
	if (len) {
		// non US ascii!
		// assume this string needs translating...
		if (!ImportTranslate::ConvertString( inFile, outFile, PR_TRUE)) {
			outFile = inFile;
			return( PR_FALSE);
		}
		else {
			return( PR_TRUE);
		}
	}
	else {
		outFile = inFile;
		return( PR_FALSE);
	}
}

PRBool nsImportMimeEncode::WriteFileName( nsCString& fName, PRBool wasTrans, const char *pTag)
{
	int			tagNum = 0;
	int			idx = 0;
	PRBool		result = PR_TRUE;
	int			len;
	nsCString	numStr;

	while ((((fName.Length() - idx) + nsCRT::strlen( pTag)) > 70) && result) {
		len = 68 - nsCRT::strlen( pTag) - 5;
		if (wasTrans) {
			if (fName.CharAt( idx + len - 1) == '%')
				len--;
			else if (fName.CharAt( idx + len - 2) == '%')
				len -= 2;
		}

		if (result)
			result = m_pOut->WriteStr( "\x09");
		if (result)
			result = m_pOut->WriteStr( pTag);
		numStr = "*";
		numStr.AppendInt( tagNum);
		if (result)
			result = m_pOut->WriteStr( (const char *)numStr);
		if (wasTrans && result)
			result = m_pOut->WriteStr( "*=");
		else if (result)
			result = m_pOut->WriteStr( "=\"");
		if (result)
			result = m_pOut->WriteData( ((const PRUint8 *)(const char *)fName) + idx, len);
		if (wasTrans && result)
			result = m_pOut->WriteStr( "\x0D\x0A");
		else if (result)
			result = m_pOut->WriteStr( "\"\x0D\x0A");
		idx += len;
		tagNum++;
	}
	
	if (idx) {
		if ((fName.Length() - idx) > 0) {
			if (result)
				result = m_pOut->WriteStr( "\x09");
			if (result)
				result = m_pOut->WriteStr( pTag);
			numStr = "*";
			numStr.AppendInt( tagNum);
			if (result)
				result = m_pOut->WriteStr( (const char *)numStr);
			if (wasTrans && result)
				result = m_pOut->WriteStr( "*=");
			else if (result)
				result = m_pOut->WriteStr( "=\"");
			if (result)
				result = m_pOut->WriteData( ((const PRUint8 *)(const char *)fName) + idx, fName.Length() - idx);
			if (wasTrans && result)
				result = m_pOut->WriteStr( "\x0D\x0A");
			else if (result)
				result = m_pOut->WriteStr( "\"\x0D\x0A");
		}
	}
	else {
		if (result)
			result = m_pOut->WriteStr( "\x09");
		if (result)
			result = m_pOut->WriteStr( pTag);
		if (wasTrans && result)
			result = m_pOut->WriteStr( "*=");
		else if (result)
			result = m_pOut->WriteStr( "=\"");
		if (result)
			result = m_pOut->WriteStr( (const char *)fName);
		if (wasTrans && result)
			result = m_pOut->WriteStr( "\x0D\x0A");
		else if (result)
			result = m_pOut->WriteStr( "\"\x0D\x0A");	
	}

	return( result);

}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
nsIImportMimeEncodeImpl::nsIImportMimeEncodeImpl()
{
	m_pOut = nsnull;
	m_pEncode = nsnull;

	NS_INIT_ISUPPORTS(); 
}

nsIImportMimeEncodeImpl::~nsIImportMimeEncodeImpl()
{
	if (m_pOut)
		delete m_pOut;
	if (m_pEncode)
		delete m_pEncode;
}


NS_METHOD nsIImportMimeEncodeImpl::Create( nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsIImportMimeEncodeImpl *it = new nsIImportMimeEncodeImpl();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF( it);
  nsresult rv = it->QueryInterface( aIID, aResult);
  NS_RELEASE( it);
  return rv;
}


NS_IMPL_ISUPPORTS1(nsIImportMimeEncodeImpl, nsIImportMimeEncode)

NS_METHOD nsIImportMimeEncodeImpl::EncodeFile(nsIFileSpec *inFile, nsIFileSpec *outFile, const char *fileName, const char *mimeType)
{
	return( Initialize( inFile, outFile, fileName, mimeType));
}

NS_METHOD nsIImportMimeEncodeImpl::DoWork(PRBool *done, PRBool *_retval)
{
	if (done && _retval && m_pEncode) {
		*_retval = m_pEncode->DoWork( done);
		return( NS_OK);
	}
	else
		return( NS_ERROR_FAILURE);
}

NS_METHOD nsIImportMimeEncodeImpl::NumBytesProcessed(PRInt32 *_retval)
{
	if (m_pEncode && _retval)
		*_retval = m_pEncode->NumBytesProcessed();
	return( NS_OK);
}

NS_METHOD nsIImportMimeEncodeImpl::DoEncoding(PRBool *_retval)
{
	if (_retval && m_pEncode) {
		PRBool	done = PR_FALSE;
		while (m_pEncode->DoWork( &done) && !done);
		*_retval = done;
		return( NS_OK);
	}
	else
		return( NS_ERROR_FAILURE);
}

NS_METHOD nsIImportMimeEncodeImpl::Initialize(nsIFileSpec *inFile, nsIFileSpec *outFile, const char *fileName, const char *mimeType)
{
	if (m_pEncode)
		delete m_pEncode;
	if (m_pOut)
		delete m_pOut;

	m_pOut = new ImportOutFile();
	m_pOut->InitOutFile( outFile);

	m_pEncode = new nsImportMimeEncode();
	m_pEncode->EncodeFile( inFile, m_pOut, fileName, mimeType);

	return( NS_OK);
}

