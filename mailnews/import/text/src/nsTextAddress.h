/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsTextAddress_h__
#define nsTextAddress_h__

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFileSpec.h"
#include "nsISupportsArray.h"
#include "nsIImportFieldMap.h"
#include "nsIImportService.h"

class nsIAddrDatabase;
class nsIMdbRow;

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

class nsTextAddress {
public:
	nsTextAddress();
	virtual ~nsTextAddress();

	nsresult	ImportAddresses( PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIAddrDatabase *pDb, nsIImportFieldMap *fieldMap, nsString& errors, PRUint32 *pProgress);
	nsresult	ImportLDIF( PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIAddrDatabase *pDb, nsString& errors, PRUint32 *pProgress);

	nsresult	DetermineDelim( nsIFileSpec *pSrc);
	char		GetDelim( void) { return( m_delim);}

	static nsresult		IsLDIFFile( nsIFileSpec *pSrc, PRBool *pIsLDIF);
	
	static nsresult		ReadRecordNumber( nsIFileSpec *pSrc, char *pLine, PRInt32 bufferSz, char delim, PRInt32 *pLineLen, PRInt32 rNum);
	static nsresult		ReadRecord( nsIFileSpec *pSrc, char *pLine, PRInt32 bufferSz, char delim, PRInt32 *pLineLen);
	static PRBool		GetField( const char *pLine, PRInt32 maxLen, PRInt32 index, nsCString& field, char delim);

private:
	nsresult		ProcessLine( const char *pLine, PRInt32 len, nsString& errors);
	void			ConvertToUnicode( const char *pStr, nsString& str);
	
	static PRBool		IsLineComplete( const char *pLine, PRInt32 len, char delim);
	static PRInt32		CountFields( const char *pLine, PRInt32 maxLen, char delim);
	static void			SanitizeSingleLine( nsCString& val);

private:
	// LDIF stuff, this wasn't originally written by me!
	nsresult	str_parse_line( char *line, char **type, char **value, int *vlen);
	char *		str_getline( char **next);
	nsresult	GetLdifStringRecord(char* buf, PRInt32 len, PRInt32& stopPos);
	nsresult	ParseLdifFile( nsIFileSpec *pSrc, PRUint32 *pProgress);
	void		AddLdifRowToDatabase( PRBool bIsList);
	void		AddLdifColToDatabase(nsIMdbRow* newRow, char* typeSlot, char* valueSlot, PRBool bIsList);
  void    ClearLdifRecordBuffer();

private:
	nsCString		m_ldifLine;
  PRInt32     m_LFCount;
  PRInt32     m_CRCount;
	char				m_delim;
	nsIAddrDatabase *	m_database;
	nsIImportFieldMap *	m_fieldMap;
	nsCOMPtr<nsIImportService> m_pService;
};



#endif /* nsTextAddress_h__ */

