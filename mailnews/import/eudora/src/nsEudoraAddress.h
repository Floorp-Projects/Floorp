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

#ifndef nsEudoraAddress_h__
#define nsEudoraAddress_h__

#include "nscore.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFileSpec.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIImportService.h"


class nsIAddrDatabase;
class CAliasEntry;
class CAliasData;


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

class nsEudoraAddress {
public:
	nsEudoraAddress();
	virtual ~nsEudoraAddress();

	// Things that must be overridden because they are platform specific.
		// retrieve the mail folder
	virtual PRBool		FindAddressFolder( nsIFileSpec *pFolder) { return( PR_FALSE);}
		// get the list of mailboxes
	virtual nsresult	FindAddressBooks( nsIFileSpec *pRoot, nsISupportsArray **ppArray) { return( NS_ERROR_FAILURE);}
		
	// Non-platform specific common stuff
		// import a mailbox
	nsresult ImportAddresses( PRUint32 *pBytes, PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIAddrDatabase *pDb, nsString& errors);


private:
	void 			EmptyAliases( void);
	void			ProcessLine( const char *pLine, PRInt32 len, nsString& errors);
	PRInt32 		CountWhiteSpace( const char *pLine, PRInt32 len);
	CAliasEntry	*	ProcessAlias( const char *pLine, PRInt32 len, nsString& errors);
	void			ProcessNote( const char *pLine, PRInt32 len, nsString& errors);
	PRInt32			GetAliasName( const char *pLine, PRInt32 len, nsCString& name);
	CAliasEntry *	ResolveAlias( nsCString& name);
	void 			ResolveEntries( nsCString& name, nsVoidArray& list, nsVoidArray& result);
	void			BuildABCards( PRUint32 *pBytes, nsIAddrDatabase *pDb);
	void			AddSingleCard( CAliasEntry *pEntry, nsVoidArray &emailList, nsIAddrDatabase *pDb);
  nsresult  AddSingleList( CAliasEntry *pEntry, nsVoidArray &emailList, nsIAddrDatabase *pDb);
  nsresult  AddGroupMembersAsCards(nsVoidArray &membersArray, nsIAddrDatabase *pDb);
  void      RememberGroupMembers(nsVoidArray &membersArray, nsVoidArray &emailList);
	PRInt32			FindAlias( nsCString& name);
	void			ExtractNoteField( nsCString& note, nsCString& field, const char *pFieldName);
	void			SanitizeValue( nsCString& val);
	void			SplitString( nsCString& val1, nsCString& val2);

public:
	static PRInt32 		CountQuote( const char *pLine, PRInt32 len);
	static PRInt32 		CountComment( const char *pLine, PRInt32 len);
	static PRInt32 		CountAngle( const char *pLine, PRInt32 len);

private:
	nsVoidArray		m_alias;
};



#endif /* nsEudoraAddress_h__ */

