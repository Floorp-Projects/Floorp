/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

