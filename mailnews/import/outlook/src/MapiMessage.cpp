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
 
#include "nscore.h"
#include <time.h>
#include "nsString.h"
#include "nsFileSpec.h"

#include "MapiDbgLog.h"
#include "MapiApi.h"
#include "MapiMessage.h"

#include "MapiMimeTypes.h"

// needed for the call the OpenStreamOnFile
extern LPMAPIALLOCATEBUFFER	gpMapiAllocateBuffer;
extern LPMAPIFREEBUFFER		gpMapiFreeBuffer;

// Sample From line: From - 1 Jan 1965 00:00:00

typedef const char * PC_S8;

static const char *	kWhitespace = "\b\t\r\n ";
static const char *	sFromLine = "From - ";
static const char *	sFromDate = "Mon Jan 1 00:00:00 1965";
static const char *	sDaysOfWeek[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *sMonths[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


CMapiMessage::CMapiMessage( LPMESSAGE lpMsg)
{
	m_lpMsg = lpMsg;
	m_pAttachTable = NULL;
	m_bMimeEncoding = FALSE;
	m_bMimeVersion = FALSE;
	m_ownsAttachFile = FALSE;
	m_whitespace = kWhitespace;
}

CMapiMessage::~CMapiMessage()
{
	if (m_pAttachTable)
		m_pAttachTable->Release();
	if (m_lpMsg)
		m_lpMsg->Release();

	ClearTempAttachFile();
}


void CMapiMessage::FormatDateTime( SYSTEMTIME & tm, nsCString& s, BOOL includeTZ)
{
	long offset = _timezone;
	s += sDaysOfWeek[tm.wDayOfWeek];
	s += ", ";
	s.AppendInt( (PRInt32) tm.wDay);
	s += " ";
	s += sMonths[tm.wMonth - 1];
	s += " ";
	s.AppendInt( (PRInt32) tm.wYear);
	s += " ";
	int val = tm.wHour;
	if (val < 10)
		s += "0";
	s.AppendInt( (PRInt32) val);
	s += ":";
	val = tm.wMinute;
	if (val < 10)
		s += "0";
	s.AppendInt( (PRInt32) val);
	s += ":";
	val = tm.wSecond;
	if (val < 10)
		s += "0";
	s.AppendInt( (PRInt32) val);
	if (includeTZ) {
		s += " ";
		if (offset < 0) {
			offset *= -1;
			s += "+";
		}
		else
			s += "-";
		offset /= 60;
		val = (int) (offset / 60);
		if (val < 10)
			s += "0";
		s.AppendInt( (PRInt32) val);
		val = (int) (offset % 60);
		if (val < 10)
			s += "0";
		s.AppendInt( (PRInt32) val);
	}
}


	// Headers - fetch will get PR_TRANSPORT_MESSAGE_HEADERS
	// or if they do not exist will build a header from
	//	PR_DISPLAY_TO, _CC, _BCC
	//	PR_SUBJECT
	//  PR_MESSAGE_RECIPIENTS
	// and PR_CREATION_TIME if needed?
void CMapiMessage::BuildHeaders( void)
{
	// Try to the to line.
	m_headers.Truncate();
	AddHeader( m_headers, PR_DISPLAY_TO, "To: ");
	AddHeader( m_headers, PR_DISPLAY_CC, "CC: ");
	AddHeader( m_headers, PR_DISPLAY_BCC, "BCC: ");
	AddDate( m_headers);
	AddSubject( m_headers);
	AddFrom( m_headers);
}

BOOL CMapiMessage::AddHeader( nsCString& str, ULONG tag, const char *pPrefix)
{
	nsCString		value;
	LPSPropValue	pVal = CMapiApi::GetMapiProperty( m_lpMsg, tag);
	if (CMapiApi::GetStringFromProp( pVal, value) && !value.IsEmpty()) {
		str.Trim( kWhitespace, PR_FALSE, PR_TRUE);
		if (!str.IsEmpty())
			str += "\x0D\x0A";
		str += pPrefix;
		str += value;
		return( TRUE);
	}	

	return( FALSE);
}

void CMapiMessage::AddSubject( nsCString& str)
{
	AddHeader( str, PR_SUBJECT, "Subject: ");	
}

void CMapiMessage::AddFrom( nsCString& str)
{
	if (!AddHeader( str, PR_SENDER_NAME, "From: "))
		AddHeader( str, PR_SENDER_EMAIL_ADDRESS, "From: ");
}

void CMapiMessage::AddDate( nsCString& str)
{
	LPSPropValue pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_MESSAGE_DELIVERY_TIME);
	if (!pVal)
		pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_CREATION_TIME);
	if (pVal) {
		SYSTEMTIME	st;
		::FileTimeToSystemTime( &(pVal->Value.ft), &st);
		CMapiApi::MAPIFreeBuffer( pVal);
		str.Trim( kWhitespace, PR_FALSE, PR_TRUE);
		if (!str.IsEmpty())
			str += "\x0D\x0A";
		str += "Date: ";
		FormatDateTime( st, str);
	}
}


void CMapiMessage::BuildFromLine( void)
{
	m_fromLine = sFromLine;
	LPSPropValue	pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_CREATION_TIME);
	if (pVal) {
		SYSTEMTIME	st;
		::FileTimeToSystemTime( &(pVal->Value.ft), &st);
		CMapiApi::MAPIFreeBuffer( pVal);
		FormatDateTime( st, m_fromLine, FALSE);
	}
	else
		m_fromLine += sFromDate;
	m_fromLine += "\x0D\x0A";

}

BOOL CMapiMessage::FetchHeaders( void)
{
	LPSPropValue	pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_TRANSPORT_MESSAGE_HEADERS);
	if (pVal && CMapiApi::IsLargeProperty( pVal)) {
		m_headers.Truncate();
		CMapiApi::GetLargeStringProperty( m_lpMsg, PR_TRANSPORT_MESSAGE_HEADERS, m_headers);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_TSTRING) && (pVal->Value.LPSZ) && (*(pVal->Value.LPSZ))) {
		m_headers = pVal->Value.LPSZ;
	}
	else {
		// Need to build the headers from the other stuff
		m_headers.Truncate();
		BuildHeaders();
	}
	
	if (pVal)
		CMapiApi::MAPIFreeBuffer( pVal);

	m_fromLine.Truncate();
	if (NeedsFromLine()) {
		BuildFromLine();
	}

	if (!m_fromLine.IsEmpty()) {
		MAPI_DUMP_STRING( m_fromLine);
	}
	MAPI_DUMP_STRING( m_headers);
	MAPI_TRACE0( "\r\n");

	ProcessHeaders();

	if (!m_headers.IsEmpty()) {
		if (!m_bHasSubject)
			AddSubject( m_headers);
		if (!m_bHasFrom)
			AddFrom( m_headers);
		if (!m_bHasDate)
			AddDate( m_headers);
		m_headers.Trim( kWhitespace, PR_FALSE, PR_TRUE);
		m_headers += "\x0D\x0A";
	}

#ifdef _DEBUG
	// CMapiApi::ListProperties( m_lpMsg);
	// MAPI_TRACE0( "--------------------\r\n");
#endif

	return( !m_headers.IsEmpty());
}

	// TRUE if a From line needs to precede the headers, FALSE
	// if the headers already include a from line	
BOOL CMapiMessage::NeedsFromLine( void)
{
	nsCString	l;
	m_headers.Left( l, 5);
	if (l.Equals("From "))
		return( FALSE);
	else
		return( TRUE);
}

BOOL CMapiMessage::IsMultipart( void)
{
	nsCString	left;
	m_mimeContentType.Left( left, 10);
	if (!left.CompareWithConversion( "multipart/", PR_TRUE))
		return( TRUE);
	return( FALSE);
}

void CMapiMessage::GenerateBoundary( void)
{
	m_mimeBoundary = "===============_NSImport_Boundary_";
	PRUint32 t = ::GetTickCount();
	nsCString	hex;
	hex.AppendInt( (PRInt32) t, 16);
	m_mimeBoundary += hex;
	m_mimeBoundary += "====";
}

BOOL CMapiMessage::GetAttachFileLoc( nsIFileSpec * pLoc)
{
	if (m_attachPath.IsEmpty())
		return( FALSE);
	pLoc->SetNativePath( m_attachPath.get());
	m_ownsAttachFile = FALSE;
	return( TRUE);
}

// Mime-Version: 1.0
// Content-Type: text/plain; charset="US-ASCII"
// Content-Type: multipart/mixed; boundary="=====================_874475278==_"

void CMapiMessage::ProcessHeaderLine( nsCString& line)
{
	PRUint32		len, start;
	nsCString		tStr;
	nsCString		left13;
	nsCString		left26;
	nsCString		left8;
	nsCString		left5;

	line.Left( left13, 13);
	line.Left( left26, 26);
	line.Left( left8, 8);
	line.Left( left5, 5);

	if (!left13.CompareWithConversion( "Mime-Version:", PR_TRUE))
		m_bMimeVersion = TRUE;
	else if (!left13.CompareWithConversion( "Content-Type:", PR_TRUE)) {
		// Note: this isn't a complete parser, the content type
		// we extract could have rfc822 comments in it
		len = 13;
		while ((len < line.Length()) && IsSpace( line.CharAt( len)))
			len++;
		start = len;
		while ((len < line.Length()) && (line.CharAt( len) != ';'))
			len++;
		line.Mid( m_mimeContentType, start, len - start);
		len++;
		// look for "boundary="
		BOOL	haveB;
		BOOL	haveC;
		while (len < line.Length()) {
			haveB = FALSE;
			haveC = FALSE;
			while ((len < line.Length()) && IsSpace( line.CharAt( len)))
				len++;
			start = len;
			while ((len < line.Length()) && (line.CharAt( len) != '='))
				len++;
			if (len - start) {
				line.Mid( tStr, start, len - start);
				if (!tStr.CompareWithConversion( "boundary", PR_TRUE))
					haveB = TRUE;
				else if (!tStr.CompareWithConversion( "charset", PR_TRUE))
					haveC = TRUE;
			}
			len++;
			while ((len < line.Length()) && IsSpace( line.CharAt( len)))
				len++;
			if ((len < line.Length()) && (line.CharAt( len) == '"')) {
				len++;
				BOOL slash = FALSE;
				tStr.Truncate();
				while (len < line.Length()) {
					if (slash) {
						slash = FALSE;
						tStr.Append(line.CharAt( len));
					}
					else if (line.CharAt( len) == '"')
						break;
					else if (line.CharAt( len) != '\\')
						tStr.Append(line.CharAt( len));
					else
						slash = TRUE;
					len++;
				}
				len++;
				if (haveB) {
					m_mimeBoundary = tStr;
					haveB = FALSE;
				}
				if (haveC) {
					m_mimeCharset = tStr;
					haveC = FALSE;
				}
			}
			tStr.Truncate();
			while ((len < line.Length()) && (line.CharAt( len) != ';')) {
				tStr.Append(line.CharAt( len));
				len++;
			}
			len++;
			if (haveB) {
				tStr.Trim( kWhitespace);
				m_mimeBoundary = tStr;
			}
			if (haveC) {
				tStr.Trim( kWhitespace);
				m_mimeCharset = tStr;
			}

		}
	}
	else if (!left26.CompareWithConversion( "Content-Transfer-Encoding:", PR_TRUE)) {
		m_bMimeEncoding = TRUE;
	}
	else if (!left8.CompareWithConversion( "Subject:", PR_TRUE))
		m_bHasSubject = TRUE;
	else if (!left5.CompareWithConversion( "From:", PR_TRUE))
		m_bHasFrom = TRUE;
	else if (!left5.CompareWithConversion( "Date: ", PR_TRUE))
		m_bHasDate = TRUE;
}

void CMapiMessage::ProcessHeaders( void)
{
	m_bHasSubject = FALSE;
	m_bHasFrom = FALSE;
	m_bHasDate = FALSE;

	PC_S8		pChar = (PC_S8) m_headers.get();
	int			start = 0;
	int			len = 0;
	nsCString	line;
	nsCString	mid;
	while (*pChar) {
		if ((*pChar == 0x0D) && (*(pChar + 1) == 0x0A)) {
			if ((*(pChar + 2) != ' ') && (*(pChar + 2) != 9)) {
				m_headers.Mid( mid, start, len);
				line += mid;
				ProcessHeaderLine( line);
				line.Truncate();
				pChar++; // subsequent increment will move pChar to the next line
				start += len;
				start += 2;
				len = -1;
			}
		}
		pChar++;
		len++;
	}

	if (!m_mimeContentType.IsEmpty() || !m_mimeBoundary.IsEmpty() || !m_mimeCharset.IsEmpty()) {
		MAPI_TRACE1( "\tDecoded mime content type: %s\r\n", (PC_S8)m_mimeContentType);
		MAPI_TRACE1( "\tDecoded mime boundary: %s\r\n", (PC_S8)m_mimeBoundary);
		MAPI_TRACE1( "\tDecoded mime charset: %s\r\n", (PC_S8)m_mimeCharset);
	}
}

BOOL CMapiMessage::FetchBody( void)
{
	m_bodyIsHtml = FALSE;
	m_body.Truncate();
	LPSPropValue	pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_BODY);
	if (!pVal) {
		// Is it html?
		pVal = CMapiApi::GetMapiProperty( m_lpMsg, 0x1013001e);
		if (pVal && CMapiApi::IsLargeProperty( pVal))
			CMapiApi::GetLargeStringProperty( m_lpMsg, 0x1013001e, m_body);
		else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_TSTRING) && (pVal->Value.LPSZ) && (*(pVal->Value.LPSZ))) {
			m_body = pVal->Value.LPSZ;
		}
		if (!m_body.IsEmpty())
			m_bodyIsHtml = TRUE;
	}
	else {
		if (pVal && CMapiApi::IsLargeProperty( pVal)) {
			CMapiApi::GetLargeStringProperty( m_lpMsg, PR_BODY, m_body);
		}
		else {
			if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_TSTRING) && (pVal->Value.LPSZ) && (*(pVal->Value.LPSZ))) {
				m_body = pVal->Value.LPSZ;
			}
		}
	}

	if (pVal)
		CMapiApi::MAPIFreeBuffer( pVal);

	MAPI_DUMP_STRING( m_body);
	MAPI_TRACE0( "\r\n");

	return( TRUE);
}

enum {
    ieidPR_ATTACH_NUM = 0,
    ieidAttachMax
};

static const SizedSPropTagArray(ieidAttachMax, ptaEid)=
{
    ieidAttachMax,
    {
        PR_ATTACH_NUM
    }
};

int CMapiMessage::CountAttachments( void)
{
	m_attachNums.RemoveAll();

	LPSPropValue	pVal = CMapiApi::GetMapiProperty( m_lpMsg, PR_HASATTACH);
	BOOL			has = TRUE;

	if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_BOOLEAN)) {
		has = (pVal->Value.b != 0);
	}
	if (pVal)
		CMapiApi::MAPIFreeBuffer( pVal);

	if (has) {
		// Get the attachment table?
		HRESULT			hr;
		LPMAPITABLE		pTable = NULL;

		hr = m_lpMsg->GetAttachmentTable( 0, &pTable);
		if (FAILED( hr))
			return( 0);
		m_pAttachTable = pTable;
		IterateAttachTable();
	}

	return( m_attachNums.GetSize());
}


BOOL CMapiMessage::IterateAttachTable( void)
{
	LPMAPITABLE lpTable = m_pAttachTable;
	ULONG rowCount;
	HRESULT hr = lpTable->GetRowCount( 0, &rowCount);
	if (!rowCount) {
		return( TRUE);
	}

	hr = lpTable->SetColumns( (LPSPropTagArray)&ptaEid, 0);
	if (FAILED(hr)) {
		MAPI_TRACE2( "SetColumns for attachment table failed: 0x%lx, %d\r\n", (long)hr, (int)hr);
		return( FALSE);
	}

	hr = lpTable->SeekRow( BOOKMARK_BEGINNING, 0, NULL);
	if (FAILED(hr)) {
		MAPI_TRACE2( "SeekRow for attachment table failed: 0x%lx, %d\r\n", (long)hr, (int)hr);
		return( FALSE);
	}

	int			cNumRows = 0;
	LPSRowSet	lpRow;
	BOOL		bResult = TRUE;
	do {
		
		lpRow = NULL;
		hr = lpTable->QueryRows( 1, 0, &lpRow);

        if(HR_FAILED(hr)) {
			MAPI_TRACE2( "QueryRows for attachment table failed: 0x%lx, %d\n", (long)hr, (int)hr);
            bResult = FALSE;
			break;
		}

        if(lpRow) {
            cNumRows = lpRow->cRows;

		    if (cNumRows) {
                LONG	aNum = lpRow->aRow[0].lpProps[ieidPR_ATTACH_NUM].Value.l;
				m_attachNums.Add( (PRUint32)aNum);
				MAPI_TRACE1( "\t\t****Attachment found - #%d\r\n", (int)aNum);
		    }
			CMapiApi::FreeProws( lpRow);		
        }

	} while ( SUCCEEDED(hr) && cNumRows && lpRow);

	return( bResult);
}

void CMapiMessage::ClearTempAttachFile( void)
{
	if (m_ownsAttachFile && !m_attachPath.IsEmpty()) {
		nsFileSpec	spec( m_attachPath.get());
		spec.Delete( PR_FALSE);
	}
	m_ownsAttachFile = FALSE;	
	m_attachPath.Truncate();
}

BOOL CMapiMessage::CopyBinAttachToFile( LPATTACH lpAttach)
{
	LPSTREAM	lpStreamFile;
	char		tPath[256];
	DWORD		len;

	
	if (!(len = ::GetTempPath( 256, tPath)) || (len > 200)) {
		len = 3;
		strcpy( tPath, "c:\\");
	}
	
	if (tPath[len - 1] != '\\')
		strcat( tPath, "\\");

	m_ownsAttachFile = FALSE;
	m_attachPath.Truncate();

	HRESULT hr = CMapiApi::OpenStreamOnFile( gpMapiAllocateBuffer, gpMapiFreeBuffer, STGM_READWRITE | STGM_CREATE | SOF_UNIQUEFILENAME,
						tPath, NULL, &lpStreamFile);
	if (HR_FAILED(hr)) {
		MAPI_TRACE1( "~~ERROR~~ OpenStreamOnFile failed - temp path: %s\r\n", tPath);
		return( FALSE);
	}
	
	m_attachPath = tPath;

	MAPI_TRACE1( "\t\t** Attachment extracted to temp file: %s\r\n", (const char *)m_attachPath);

	BOOL		bResult = TRUE;
	LPSTREAM	lpAttachStream;
	hr = lpAttach->OpenProperty( PR_ATTACH_DATA_BIN, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpAttachStream);

	if (HR_FAILED( hr)) {
		MAPI_TRACE0( "~~ERROR~~ OpenProperty failed for PR_ATTACH_DATA_BIN.\r\n");
		lpAttachStream = NULL;
		bResult = FALSE;
	}
	else {
		STATSTG		st;
		hr = lpAttachStream->Stat( &st, STATFLAG_NONAME);
		if (HR_FAILED( hr)) {
			MAPI_TRACE0( "~~ERROR~~ Stat failed for attachment stream\r\n");
			bResult = FALSE;
		}
		else {
			hr = lpAttachStream->CopyTo( lpStreamFile, st.cbSize, NULL, NULL);
			if (HR_FAILED( hr)) {
				MAPI_TRACE0( "~~ERROR~~ Attach Stream CopyTo temp file failed.\r\n");
				bResult = FALSE;
			}
		}
	}

	if (lpAttachStream)
		lpAttachStream->Release();
	lpStreamFile->Release();
	if (!bResult) {
		nsFileSpec	spec( m_attachPath.get());
		spec.Delete( PR_FALSE);
	}
	else
		m_ownsAttachFile = TRUE;

	return( bResult);
}

BOOL CMapiMessage::GetAttachmentInfo( int idx)
{
	ClearTempAttachFile();

	BOOL	bResult = TRUE;
	if ((idx < 0) || (idx >= (int)m_attachNums.GetSize())) {
		return( FALSE);	
	}

	DWORD		aNum = m_attachNums.GetAt( idx);
	LPATTACH	lpAttach = NULL;
	HRESULT		hr = m_lpMsg->OpenAttach( aNum, NULL, 0, &lpAttach);
	if (HR_FAILED( hr)) {
		MAPI_TRACE2( "\t\t****Attachment error, unable to open attachment: %d, 0x%lx\r\n", idx, hr);
		return( FALSE);
	}
	
	LPSPropValue	pVal;
	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_MIME_TAG);
	if (pVal)
		CMapiApi::GetStringFromProp( pVal, m_attachMimeType);
	else
		m_attachMimeType.Truncate();

	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_METHOD);
	if (pVal) {
		LONG	aMethod = CMapiApi::GetLongFromProp( pVal);
		if ((aMethod == ATTACH_BY_REF_ONLY) || (aMethod == ATTACH_BY_REFERENCE) || (aMethod == ATTACH_BY_REF_RESOLVE)) {
			m_attachPath.Truncate();
			pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_PATHNAME);
			if (pVal)
				CMapiApi::GetStringFromProp( pVal, m_attachPath);
			MAPI_TRACE2( "\t\t** Attachment #%d by ref: %s\r\n", idx, (const char *)m_attachPath);
			m_ownsAttachFile = FALSE;
		}
		else if (aMethod == ATTACH_BY_VALUE) {
			MAPI_TRACE1( "\t\t** Attachment #%d by value.\r\n", idx);
			bResult = CopyBinAttachToFile( lpAttach);
		}
		else if (aMethod == ATTACH_OLE) {
			MAPI_TRACE1( "\t\t** Attachment #%d by OLE - yuck!!!\r\n", idx);
		}
		else if (aMethod == ATTACH_EMBEDDED_MSG) {
			MAPI_TRACE1( "\t\t** Attachment #%d by Embedded Message??\r\n", idx);
		}
		else {
			MAPI_TRACE2( "\t\t** Attachment #%d unknown attachment method - 0x%lx\r\n", idx, aMethod);
			bResult = FALSE;
		}
	}
	else
		bResult = FALSE;
	
	nsCString	fName, fExt;
	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_FILENAME);
	if (pVal)
		CMapiApi::GetStringFromProp( pVal, fName);
	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_EXTENSION);
	if (pVal)
		CMapiApi::GetStringFromProp( pVal, fExt);
	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_SIZE);
	long sz = 0;
	if (pVal)
		sz = CMapiApi::GetLongFromProp( pVal);

	/*
		// I have no idea how this tag is used, how to interpret it's value, etc.
		// Fortunately, the Microsoft documentation is ABSOLUTELY NO HELP AT ALL.  In fact,
		// if one goes by the docs and sample code, this tag is completely 100% useless.  I'm
		// sure it has some important meaning which will one day be obvious, but for now,
		// it is ignored.
	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_TAG);
	if (pVal) {
		::MAPIFreeBuffer( pVal);
	}
	*/

	MAPI_TRACE1( "\t\t\t--- Mime type: %s\r\n", (const char *)m_attachMimeType);
	MAPI_TRACE2( "\t\t\t--- File name: %s, extension: %s\r\n", (const char *)fName, (const char *)fExt);
	MAPI_TRACE1( "\t\t\t--- Size: %ld\r\n", sz);

	if (fExt.IsEmpty()) {
		int idx = fName.RFindChar( '.');
		if (idx != -1)
			fName.Right( fExt, fName.Length() - idx);
	}
	
	if ((fName.RFindChar( '.') == -1) && !fExt.IsEmpty()) {
		fName += ".";
		fName += fExt;
	}

	m_attachFileName = fName;

	if (m_attachMimeType.IsEmpty()) {
		PRUint8 *pType = NULL;
		if (!fExt.IsEmpty()) {
			pType = CMimeTypes::GetMimeType( fExt);
		}
		if (pType)
			m_attachMimeType = (PC_S8)pType;
		else
			m_attachMimeType = "application/octet-stream";
	}

	pVal = CMapiApi::GetMapiProperty( lpAttach, PR_ATTACH_TRANSPORT_NAME);
	if (pVal) {
		CMapiApi::GetStringFromProp( pVal, fName);
		MAPI_TRACE1( "\t\t\t--- Transport name: %s\r\n", (const char *)fName);
	}

	lpAttach->Release();

	return( bResult);
}

void nsSimpleUInt32Array::Allocate( void)
{
	if (m_used < m_allocated)
		return;
	if (!m_pData) {
		m_pData = new PRUint32[m_growBy];
		m_allocated = m_growBy;
	}
	else {
		m_allocated += m_growBy;
		PRUint32 *pData = new PRUint32[m_allocated];
		nsCRT::memcpy( pData, m_pData, (m_allocated - m_growBy) * sizeof( PRUint32));
		delete [] m_pData;
		m_pData = pData;
	}
}
