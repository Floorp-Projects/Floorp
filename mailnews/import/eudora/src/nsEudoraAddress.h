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

