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

#include "nsCOMPtr.h"
#include "nsEudoraAddress.h"

#include "nsIAddrDatabase.h"
#include "nsABBaseCID.h"
#include "nsIAbCard.h"
#include "nsIServiceManager.h"
#include "nsEudoraImport.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kAbCardPropertyCID,	NS_ABCARDPROPERTY_CID);

#include "EudoraDebugLog.h"

#define	kWhitespace	" \t\b\r\n"

// If we get a line longer than 16K it's just toooooo bad!
#define kEudoraAddressBufferSz	(16 * 1024)


#ifdef IMPORT_DEBUG
void DumpAliasArray( nsVoidArray& a);
#endif

class CAliasData {
public:
	CAliasData() {}
	~CAliasData() {}
	
	PRBool Process( const char *pLine, PRInt32 len);
	
public:
	nsCString	m_nickName;
	nsCString	m_realName;
	nsCString	m_email;
};

class CAliasEntry {
public:
	CAliasEntry( nsCString& name) { m_name = name;}
	~CAliasEntry() { EmptyList();}
	
	void EmptyList( void) {
		CAliasData *pData;
		for (PRInt32 i = 0; i < m_list.Count(); i++) {
			pData = (CAliasData *)m_list.ElementAt( i);
			delete pData;
		}
		m_list.Clear();
	}

public:
	nsCString	m_name;
	nsVoidArray	m_list;
	nsCString	m_notes;
};

nsEudoraAddress::nsEudoraAddress()
{
}

nsEudoraAddress::~nsEudoraAddress()
{
	EmptyAliases();
}


nsresult nsEudoraAddress::ImportAddresses( PRUint32 *pBytes, PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIAddrDatabase *pDb, nsString& errors)
{
	// Open the source file for reading, read each line and process it!
	
	EmptyAliases();
	
	nsresult rv = pSrc->OpenStreamForReading();
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error opening address file for reading\n");
		return( rv);
	}
	
	char *pLine = new char[kEudoraAddressBufferSz];
	PRBool	wasTruncated;
	PRBool	eof = PR_FALSE;
	rv = pSrc->Eof( &eof);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error checking address file for eof\n");
		pSrc->CloseStream();
		return( rv);
	}
	
	while (!(*pAbort) && !eof && NS_SUCCEEDED( rv)) {
		wasTruncated = PR_FALSE;
		rv = pSrc->ReadLine( &pLine, kEudoraAddressBufferSz, &wasTruncated);
		if (wasTruncated)
			pLine[kEudoraAddressBufferSz - 1] = 0;
		if (NS_SUCCEEDED( rv)) {
			PRInt32	len = nsCRT::strlen( pLine);
			ProcessLine( pLine, len, errors);
			rv = pSrc->Eof( &eof);
			if (pBytes)
				*pBytes += (len / 2);
		}
	}
	
	rv = pSrc->CloseStream();
	
	delete [] pLine;

	if (!eof) {
		IMPORT_LOG0( "*** Error reading the address book, didn't reach the end\n");
		return( NS_ERROR_FAILURE);
	}
	
	// Run through the alias array and make address book entries...
	#ifdef IMPORT_DEBUG
	DumpAliasArray( m_alias);
	#endif
	
	BuildABCards( pBytes, pDb);
	
	return( NS_OK);
}


PRInt32 nsEudoraAddress::CountWhiteSpace( const char *pLine, PRInt32 len)
{
	PRInt32		cnt = 0;
	while (len && ((*pLine == ' ') || (*pLine == '\t'))) {
		len--;
		pLine++;
		cnt++;
	}
	
	return( cnt);
}

void nsEudoraAddress::EmptyAliases( void)
{
	CAliasEntry *pData;
	for (PRInt32 i = 0; i < m_alias.Count(); i++) {
		pData = (CAliasEntry *)m_alias.ElementAt( i);
		delete pData;
	}
	m_alias.Clear();
}

void nsEudoraAddress::ProcessLine( const char *pLine, PRInt32 len, nsString& errors)
{
	if (len < 6)
		return;
	
	PRInt32	cnt;
	CAliasEntry	*pEntry;
	
	if (!nsCRT::strncmp( pLine, "alias", 5)) {
		pLine += 5;
		len -= 5;
		cnt = CountWhiteSpace( pLine, len);
		if (cnt) {
			pLine += cnt;
			len -= cnt;
			if ((pEntry = ProcessAlias( pLine, len, errors)) != nsnull) {
				m_alias.AppendElement( pEntry);
			}
		}
	}
	else if (!nsCRT::strncmp( pLine, "note", 4)) {
		pLine += 4;
		len -= 4;
		cnt = CountWhiteSpace( pLine, len);
		if (cnt) {
			pLine += cnt;
			len -= cnt;
			ProcessNote( pLine, len, errors);
		}
	}
		
	// as far as I know everything must be on one line
	// if not, then I need to add a state variable.
}

PRInt32 nsEudoraAddress::GetAliasName( const char *pLine, PRInt32 len, nsCString& name)
{
	name.Truncate();
	if (!len)
		return( 0);
	const char *pStart = pLine;
	char	end[2] = {' ', '\t'};
	if (*pLine == '"') {
		pLine++;
		pStart++;
		len--;
		end[0] = '"';
		end[1] = 0;
	}
	
	PRInt32 cnt = 0;
	while (len) {
		if ((*pLine == end[0]) || (*pLine == end[1]))
			break;
		len--;
		pLine++;
		cnt++;
	}
	
	if (cnt)
		name.Append( pStart, cnt);
	
	if (end[0] == '"') {
		cnt++;
		if (len && (*pLine == '"')) {
			cnt++;
			pLine++;
			len--;
		}
	}
	
	cnt += CountWhiteSpace( pLine, len);
	
	return( cnt);
}


CAliasEntry *nsEudoraAddress::ProcessAlias( const char *pLine, PRInt32 len, nsString& errors)
{
	nsCString	name;
	PRInt32		cnt = GetAliasName( pLine, len, name);
	pLine += cnt;
	len -= cnt;
	if (!cnt || !len)
		return( nsnull);

	// we have 3 known forms of addresses in Eudora
	// 1) real name <email@address>
	// 2) email@address
	// 3) <email@address>
	// 4) real name email@address
	// 5) <email@address> (Real name)
	
	CAliasEntry *pEntry = new CAliasEntry( name);
	
	// Theoretically, an alias is just an RFC822 email adress, but it may contain
	// an alias to another alias as the email!  I general, it appears close
	// but unfortunately not exact so we can't use the nsIMsgHeaderParser to do
	// the work for us!
	
	// Very big bummer!
	
	const char *pStart;	
	PRInt32		tLen;
	nsCString	alias;
	
	while ( len) {
		pStart = pLine;
		cnt = 0;
		while (len && (*pLine != ',')) {
			if (*pLine == '"') {
				tLen = CountQuote( pLine, len);
				pLine += tLen;
				len -= tLen;
				cnt += tLen;
			}
			else if (*pLine == '(') {
				tLen = CountComment( pLine, len);
				pLine += tLen;
				len -= tLen;
				cnt += tLen;
			}
			else if (*pLine == '<') {
				tLen = CountAngle( pLine, len);
				pLine += tLen;
				len -= tLen;
				cnt += tLen;
			}
			else {
				cnt++;
				pLine++;
				len--;
			}
		}
		if (cnt) {
			CAliasData *pData = new CAliasData();
			if (pData->Process( pStart, cnt)) {
				pEntry->m_list.AppendElement( pData);
			}
			else
				delete pData;
		}
		
		if (len && (*pLine == ',')) {
			pLine++;
			len--;
		}
	}
	
	if (pEntry->m_list.Count())
		return( pEntry);
	
	// TLR: FIXME: Put an error string in the log indicating this alias could not be parsed.
	
	delete pEntry;
	return( nsnull);	
}


void nsEudoraAddress::ProcessNote( const char *pLine, PRInt32 len, nsString& errors)
{
	nsCString	name;
	PRInt32		cnt = GetAliasName( pLine, len, name);
	pLine += cnt;
	len -= cnt;
	if (!cnt || !len)
		return;
	
	// Find the alias for this note and store the note data there!
	CAliasEntry *pEntry = nsnull;
	PRInt32	idx = FindAlias( name);	
	if (idx == -1)
		return;

	pEntry = (CAliasEntry *) m_alias.ElementAt( idx);
	pEntry->m_notes.Append( pLine, len);
	pEntry->m_notes.Trim( kWhitespace);
}



PRInt32 nsEudoraAddress::CountQuote( const char *pLine, PRInt32 len)
{
	if (!len)
		return( 0);
		
	PRInt32 cnt = 1;
	pLine++;
	len--;
	
	while (len && (*pLine != '"')) {
		cnt++;
		len--;
		pLine++;
	}
	
	if (len)
		cnt++;
	return( cnt);
}


PRInt32 nsEudoraAddress::CountAngle( const char *pLine, PRInt32 len)
{
	if (!len)
		return( 0);
		
	PRInt32 cnt = 1;
	pLine++;
	len--;
	
	while (len && (*pLine != '>')) {
		cnt++;
		len--;
		pLine++;
	}
	
	if (len)
		cnt++;
	return( cnt);
}

PRInt32 nsEudoraAddress::CountComment( const char *pLine, PRInt32 len)
{
	if (!len)
		return( 0);
	
	PRInt32	cCnt;
	PRInt32 cnt = 1;
	pLine++;
	len--;
	
	while (len && (*pLine != ')')) {
		if (*pLine == '(') {
			cCnt = CountComment( pLine, len);	
			cnt += cCnt;
			pLine += cCnt;
			len -= cCnt;
		}
		else {
			cnt++;
			len--;
			pLine++;
		}
	}
	
	if (len)
		cnt++;
	return( cnt);
}

/*
	nsCString	m_nickName;
	nsCString	m_realName;
	nsCString	m_email;
*/

PRBool CAliasData::Process( const char *pLine, PRInt32 len)
{
	// Extract any comments first!
	nsCString	str;
	
	const char *pStart = pLine;
	PRInt32		tCnt = 0;
	PRInt32		cnt = 0;
	PRInt32		max = len;
	PRBool		endCollect = PR_FALSE;
	
	while (max) {
		if (*pLine == '"') {
			if (tCnt && !endCollect) {
				str.Trim( kWhitespace);
				if (str.Length())
					str.Append( " ", 1);
				str.Append( pStart, tCnt);
			}
			cnt = nsEudoraAddress::CountQuote( pLine, max);
			if ((cnt > 2) && m_realName.IsEmpty()) {
				m_realName.Append( pLine + 1, cnt - 2);
			}
			pLine += cnt;
			max -= cnt;
			pStart = pLine;
			tCnt = 0;
		}
		else if (*pLine == '<') {
			if (tCnt && !endCollect) {
				str.Trim( kWhitespace);
				if (str.Length())
					str.Append( " ", 1);
				str.Append( pStart, tCnt);
			}
			cnt = nsEudoraAddress::CountAngle( pLine, max);
			if ((cnt > 2) && m_email.IsEmpty()) {
				m_email.Append( pLine + 1, cnt - 2);
			}
			pLine += cnt;
			max -= cnt;
			pStart = pLine;
			tCnt = 0;
			endCollect = PR_TRUE;
		}
		else if (*pLine == '(') {
			if (tCnt && !endCollect) {
				str.Trim( kWhitespace);
				if (str.Length())
					str.Append( " ", 1);
				str.Append( pStart, tCnt);
			}
			cnt = nsEudoraAddress::CountComment( pLine, max);
			if (cnt > 2) {
				if (!m_realName.IsEmpty() && m_nickName.IsEmpty())
					m_nickName = m_realName;
				m_realName.Truncate();
				m_realName.Append( pLine + 1, cnt - 2);
			}
			pLine += cnt;
			max -= cnt;
			pStart = pLine;
			tCnt = 0;
		}
		else {
			tCnt++;
			pLine++;
			max--;
		}
	}
	
	if (tCnt) {
		str.Trim( kWhitespace);
		if (str.Length())
			str.Append( " ", 1);
		str.Append( pStart, tCnt);
	}
	
	str.Trim( kWhitespace);
	
	if (m_realName.Length() && m_email.Length())
		return( PR_TRUE);
	
	// now we should have a string with any remaining non-delimitted text
	// we assume that the last token is the email
	// anything before that is realName
	if (m_email.Length()) {
		m_realName = str;
		return( PR_TRUE);
	}
	
	tCnt = str.RFindChar( ' ');
	if (tCnt == -1) {
		if (str.Length()) {
			m_email = str;
			return( PR_TRUE);
		}
		return( PR_FALSE);
	}
	
	str.Right( m_email, str.Length() - tCnt - 1);
	str.Left( m_realName, tCnt);
	m_realName.Trim( kWhitespace);
	m_email.Trim( kWhitespace);
	
	return( (m_email.Length() != 0));
}

#ifdef IMPORT_DEBUG
void DumpAliasArray( nsVoidArray& a)
{
	CAliasEntry *pEntry;
	CAliasData *pData;
	
	PRInt32 cnt = a.Count();
	IMPORT_LOG1( "Alias list size: %ld\n", cnt);
	for (PRInt32 i = 0; i < cnt; i++) {
		pEntry = (CAliasEntry *)a.ElementAt( i);
		IMPORT_LOG1( "\tAlias: %s\n", pEntry->m_name.get());
		if (pEntry->m_list.Count() > 1) {
			IMPORT_LOG1( "\tList count #%ld\n", pEntry->m_list.Count());
			for (PRInt32 j = 0; j < pEntry->m_list.Count(); j++) {
				pData = (CAliasData *) pEntry->m_list.ElementAt( j);
				IMPORT_LOG0( "\t\t--------\n");
				IMPORT_LOG1( "\t\temail: %s\n", pData->m_email.get());
				IMPORT_LOG1( "\t\trealName: %s\n", pData->m_realName.get());
				IMPORT_LOG1( "\t\tnickName: %s\n", pData->m_nickName.get());
			}
		}
		else {
			pData = (CAliasData *) pEntry->m_list.ElementAt( 0);
			IMPORT_LOG1( "\t\temail: %s\n", pData->m_email.get());
			IMPORT_LOG1( "\t\trealName: %s\n", pData->m_realName.get());
			IMPORT_LOG1( "\t\tnickName: %s\n", pData->m_nickName.get());
		}	
	}
}
#endif

CAliasEntry *nsEudoraAddress::ResolveAlias( nsCString& name)
{
	PRInt32	max = m_alias.Count();
	CAliasEntry *pEntry;
	for (PRInt32 i = 0; i < max; i++) {
		pEntry = (CAliasEntry *) m_alias.ElementAt( i);
		if (!name.CompareWithConversion( pEntry->m_name.get(), PR_TRUE))
			return( pEntry);
	}
	
	return( nsnull);
}

void nsEudoraAddress::ResolveEntries( nsCString& name, nsVoidArray& list, nsVoidArray& result)
{
	/* a safe-guard against recursive entries */
	if (result.Count() > m_alias.Count())
		return;
		
	PRInt32			max = list.Count();
	PRInt32			i;
	CAliasData *	pData;
	CAliasEntry *	pEntry;
	for (i = 0; i < max; i++) {
		pData = (CAliasData *)list.ElementAt( i);
		// resolve the email to an existing alias!
		if (name.CompareWithConversion( pData->m_email.get(), PR_TRUE) && ((pEntry = ResolveAlias( pData->m_email)) != nsnull)) {
			// This new entry has all of the entries for this puppie.
			// Resolve all of it's entries!
			ResolveEntries( pEntry->m_name, pEntry->m_list, result);
		}
		else {
			result.AppendElement( pData);
		}
	}
}

PRInt32 nsEudoraAddress::FindAlias( nsCString& name)
{
	CAliasEntry *	pEntry;
	PRInt32			max = m_alias.Count();
	PRInt32			i;
	
	// First off, run through the list and build person cards - groups/lists have to be done later
	for (i = 0; i < max; i++) {
		pEntry = (CAliasEntry *) m_alias.ElementAt( i);
		if (pEntry->m_name == name)
			return( i);
	}

	return( -1);
}

void nsEudoraAddress::BuildABCards( PRUint32 *pBytes, nsIAddrDatabase *pDb)
{
	CAliasEntry *	pEntry;
	CAliasData *	pData;
	PRInt32			max = m_alias.Count();
	PRInt32			i;
	PRInt32			numAddrs;
	nsCString		email;
	nsVoidArray		entries;
	
	// First off, run through the list and build person cards - groups/lists have to be done later
	for (i = 0; i < max; i++) {
		pEntry = (CAliasEntry *) m_alias.ElementAt( i);
		ResolveEntries( pEntry->m_name, pEntry->m_list, entries);		
		numAddrs = entries.Count();
		if (numAddrs == 1) {
			pData = (CAliasData *)entries.ElementAt( 0);
			BuildSingleCard( pEntry, pData, pDb);
		}
		entries.Clear();

		if (pBytes) {
			// This isn't exact but it will get us close enough
			*pBytes += (pEntry->m_name.Length() + pEntry->m_notes.Length() + 10);
		}
	}
}

void nsEudoraAddress::ExtractNoteField( nsCString& note, nsCString& value, const char *pFieldName)
{
	value.Truncate( 0);
	nsCString	field("<");
	field.Append( pFieldName);
	field.Append( ':');

	/* 
		this is a bit of a cheat, but there's no reason it won't work
		fine for us, even better than Eudora in some cases!
	 */
	
	PRInt32 idx = note.Find( field);
	if (idx != -1) {
		idx += field.Length();
		PRInt32 endIdx = note.FindChar( '>', PR_FALSE, idx);
		if (endIdx == -1)
			endIdx = note.Length() - 1;
		note.Mid( value, idx, endIdx - idx);
		idx -= field.Length();
		nsCString tempL;
		if (idx)
			note.Left( tempL, idx);
		nsCString tempR;
		note.Right( tempR, note.Length() - endIdx - 1);
		note = tempL;
		note.Append( tempR);
	}
}

void nsEudoraAddress::SanitizeValue( nsCString& val)
{
	val.ReplaceSubstring( "\x0D\x0A", ", ");
	val.ReplaceChar( 13, ',');
	val.ReplaceChar( 10, ',');
}

void nsEudoraAddress::SplitString( nsCString& val1, nsCString& val2)
{
	nsCString	temp;

	// Find the last line if there is more than one!
	PRInt32 idx = val1.RFind( "\x0D\x0A");
	PRInt32	cnt = 2;
	if (idx == -1) {
		cnt = 1;
		idx = val1.RFindChar( 13);
	}
	if (idx == -1)
		idx= val1.RFindChar( 10);
	if (idx != -1) {
		val1.Right( val2, val1.Length() - idx - cnt);
		val1.Left( temp, idx);
		val1 = temp;
		SanitizeValue( val1);
	}
}

void nsEudoraAddress::BuildSingleCard( CAliasEntry *pEntry, CAliasData *pData, nsIAddrDatabase *pDb)
{
	// we have a nickname always
	// a real name sometimes
	// an email address always
	
	// anything else found in notes!
	
	nsIMdbRow* newRow = nsnull;
	pDb->GetNewRow( &newRow); 
	
	/*
		
	*/

	nsCString	displayName(pData->m_realName);
	nsCString	name;
	nsCString	fax;
	nsCString	phone;
	nsCString	address;
	nsCString	address2;
	nsCString	note(pEntry->m_notes);
	if (note.Length() > 0) {
		ExtractNoteField( note, fax, "fax");
		ExtractNoteField( note, phone, "phone");
		ExtractNoteField( note, address, "address");
		ExtractNoteField( note, name, "name");
	}
	if (displayName.IsEmpty())
		displayName = name;
	if (displayName.IsEmpty())
		displayName = pEntry->m_name;
	
	address.ReplaceSubstring( "\x03", "\x0D\x0A");
	SplitString( address, address2);
	note.ReplaceSubstring( "\x03", "\x0D\x0A");
	fax.ReplaceSubstring( "\x03", " ");
	phone.ReplaceSubstring( "\x03", " ");
	name.ReplaceSubstring( "\x03", " ");
	
	if (newRow) {
		nsAutoString uniStr;

		ConvertToUnicode( displayName.get(), uniStr);
		pDb->AddDisplayName( newRow, NS_ConvertUCS2toUTF8(uniStr).get());

		ConvertToUnicode( pEntry->m_name.get(), uniStr);
		pDb->AddNickName( newRow, NS_ConvertUCS2toUTF8(uniStr).get());

		ConvertToUnicode( pData->m_email.get(), uniStr);
		pDb->AddPrimaryEmail( newRow, NS_ConvertUCS2toUTF8(uniStr).get());

		if (!fax.IsEmpty()) {
			ConvertToUnicode( fax.get(), uniStr);
			pDb->AddFaxNumber( newRow, NS_ConvertUCS2toUTF8(uniStr).get());
		}

		if (!phone.IsEmpty()) {
			ConvertToUnicode( phone.get(), uniStr);
			pDb->AddHomePhone( newRow, NS_ConvertUCS2toUTF8(uniStr).get());
		}

		if (!address.IsEmpty()) {
			ConvertToUnicode( address.get(), uniStr);
			pDb->AddHomeAddress( newRow, NS_ConvertUCS2toUTF8(uniStr).get());
		}

		if (!address2.IsEmpty()) {
			ConvertToUnicode( address2.get(), uniStr);
			pDb->AddHomeAddress2( newRow, NS_ConvertUCS2toUTF8(uniStr).get());
		}

		if (!note.IsEmpty()) {
			ConvertToUnicode( note.get(), uniStr);
			pDb->AddNotes( newRow, NS_ConvertUCS2toUTF8(uniStr).get());
		}

		pDb->AddCardRowToDB( newRow);

		IMPORT_LOG1( "Added card to db: %s\n", displayName.get());
	}		
}


