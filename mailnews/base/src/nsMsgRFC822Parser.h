/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/********************************************************************************************************
 
   Interface for parsing RFC-822 addresses.
 
*********************************************************************************************************/

#ifndef nsMSGRFCPARSER_h__
#define nsMSGRFCPARSER_h__

#include "msgCore.h"
#include "nsIMsgRFC822Parser.h" /* include the interface we are going to support */

 /* 
  * RFC-822 parser
  */ 

 class nsMsgRFC822Parser: public nsIMsgRFC822Parser 
 {
 public: 
	 nsMsgRFC822Parser();
	 virtual ~nsMsgRFC822Parser();

	 /* this macro defines QueryInterface, AddRef and Release for this class */
	 NS_DECL_ISUPPORTS

	/* Given a string which contains a list of RFC822 addresses, parses it into
	   their component names and mailboxes.

       The returned value is the number of addresses, or a negative error code;
       the names and addresses are returned into the provided pointers as
       consecutive null-terminated strings.  It is up to the caller to free them.
       Note that some of the strings may be zero-length.

       Either of the provided pointers may be NULL if the caller is not interested
       in those components.
    */
	NS_IMETHOD ParseRFC822Addresses (const char *line, char **names, char **addresses, PRUint32& numAddresses);
	NS_IMETHOD ParseRFC822Addresses_Intl (PRInt16 csid, const char *line, char **names, char **addresses, PRUint32& numAddresses);

	/* Given a string which contains a list of RFC822 addresses, returns a
	   comma-seperated list of just the `mailbox' portions.

       Caller must call PL_Free on mailboxes string returned by these calls.
    */
	NS_IMETHOD ExtractRFC822AddressMailboxes (const char *line, char ** mailboxes);
    NS_IMETHOD ExtractRFC822AddressMailboxes_Intl (PRInt16 csid, const char *line, char ** mailboxes);


	/* Given a string which contains a list of RFC822 addresses, returns a
	   comma-seperated list of just the `user name' portions.  If any of
	   the addresses doesn't have a name, then the mailbox is used instead.

       Caller must call PL_Free on userNames
	 */
	NS_IMETHOD ExtractRFC822AddressNames (const char *line, char ** userNames);
	NS_IMETHOD ExtractRFC822AddressNames_Intl (PRInt16 csid, const char *line, char ** userNames);

	/* Like MSG_ExtractRFC822AddressNames(), but only returns the first name
	   in the list, if there is more than one. 

	   Caller must call PL_Free on the returned firstName string.
	 */
	NS_IMETHOD ExtractRFC822AddressName (const char *line, char ** firstName);
	NS_IMETHOD ExtractRFC822AddressName_Intl (PRInt16 csid, const char *line, char ** firstName);

	/* Given a string which contains a list of RFC822 addresses, returns a new
	   string with the same data, but inserts missing commas, parses and reformats
	   it, and wraps long lines with newline-tab.

       Caller must call PL_Free on the returned string
	 */
	NS_IMETHOD ReformatRFC822Addresses (const char *line, char ** reformattedAddress);
	NS_IMETHOD ReformatRFC822Addresses_Intl (PRInt16 csid, const char *line, char ** reformattedAddress);

	/* Returns a copy of ADDRS which may have had some addresses removed.
	   Addresses are removed if they are already in either ADDRS or OTHER_ADDRS.
	   (If OTHER_ADDRS contain addresses which are not in ADDRS, they are not
	   added.  That argument is for passing in addresses that were already
	   mentioned in other header fields.)

	   Addresses are considered to be the same if they contain the same mailbox
	   part (case-insensitive.)  Real names and other comments are not compared.

	   removeAliasesToMe allows the address parser to use the preference which
	   contains regular expressions which also mean 'me' for the purpose of
	   stripping the user's email address(es) out of addrs

       Caller must call PL_Free on outString.
	 */
	NS_IMETHOD RemoveDuplicateAddresses (const char *addrs, const char *other_addrs, PRBool removeAliasesToMe, char ** outString);
	NS_IMETHOD RemoveDuplicateAddresses_Intl (PRInt16 csid, const char *addrs, const char *other_addrs, PRBool removeAliasesToMe, char ** outString);


	/* Given an e-mail address and a person's name, cons them together into a
	   single string of the form "name <address>", doing all the necessary quoting.
	   A new string is returned, which you must free when you're done with it.

       Caller must call PL_Free on fullAddress
	 */
	NS_IMETHOD MakeFullAddress (const char* name, const char* addr, char ** fullAddress);
	NS_IMETHOD MakeFullAddress_Intl (PRInt16 csid, const char* name, const char* addr, char ** fullAddress);

	/* MSG_ParseRFC822Addresses returns quoted parsable addresses
	   This function removes the quoting if you want to show the
	   names to users. e.g. summary file, address book
	 */
	NS_IMETHOD UnquotePhraseOrAddr (const char *line, char** lineout);
	NS_IMETHOD UnquotePhraseOrAddr_Intl (PRInt16 csid, const char *line, char** lineout);

	private:
 }; 

 /* this function will be used by the factory to generate an RFC-822 Parser....*/
 extern nsresult NS_NewRFC822Parser(const nsIID &aIID, void ** aInstancePtrResult);

#endif /* nsMSGRFCPARSER_h__ */
