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

#ifndef nsIMSGRFC822PARSER_h__
#define nsIMSGRFC822PARSER_h__

#include "nsISupports.h" 

#define NS_IMSGRFC822PARSER_IID \
{ /* 10A88A11-729E-11d2-804A-006008128C4E */ \
0x10a88a11, 0x729e, 0x11d2, \
{0x80, 0x4a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

 /* 
  * nsIMsgRFCParser Interface declaration 
  */ 

class nsIMsgRFC822Parser: public nsISupports { 
public: 

  static const nsIID& IID(void) { static nsIID iid = NS_IMSGRFC822PARSER_IID; return iid; }
  
	/* Given a string which contains a list of RFC822 addresses, parses it into
	   their component names and mailboxes.

       The returned value is the number of addresses, or a negative error code;
       the names and addresses are returned into the provided pointers as
       consecutive null-terminated strings.  It is up to the caller to free them.
       Note that some of the strings may be zero-length.

       The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

       Either of the provided pointers may be NULL if the caller is not interested
       in those components.
    */
	NS_IMETHOD ParseRFC822Addresses (const char *charset, const char *line, char **names, char **addresses, PRUint32& numAddresses) = 0;

	/* Given a string which contains a list of RFC822 addresses, returns a
	   comma-seperated list of just the `mailbox' portions.

     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

	   Caller must call PL_strfree on returnValue (which is allocated by the function)
    */
	NS_IMETHOD ExtractRFC822AddressMailboxes (const char *charset, const char *line, char ** mailboxes) = 0;


	/* Given a string which contains a list of RFC822 addresses, returns a
	   comma-seperated list of just the `user name' portions.  If any of
	   the addresses doesn't have a name, then the mailbox is used instead.

     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

	   Caller must PL_strfree usernames.
	 */
	NS_IMETHOD ExtractRFC822AddressNames (const char *charset, const char *line, char ** userNames) = 0;

	/* Like MSG_ExtractRFC822AddressNames(), but only returns the first name
	   in the list, if there is more than one. 
	   
     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

	   Caller must call PL_strfree on name
	 */
	NS_IMETHOD ExtractRFC822AddressName (const char *charset, const char *line, char ** name) = 0;

	/* Given a string which contains a list of RFC822 addresses, returns a new
	   string with the same data, but inserts missing commas, parses and reformats
	   it, and wraps long lines with newline-tab.

     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

       Caller must call PL_strfree on reformattedAddress
	 */
	NS_IMETHOD ReformatRFC822Addresses (const char *charset, const char *line, char ** reformattedAddress) = 0;

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

     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

       Caller must call PL_strfree on newAddress which is the return value.
	 */
	NS_IMETHOD RemoveDuplicateAddresses (const char *charset, const char *addrs, const char *other_addrs, PRBool removeAliasesToMe, char ** newAddress) = 0;


	/* Given an e-mail address and a person's name, cons them together into a
	   single string of the form "name <address>", doing all the necessary quoting.
	   A new string is returned, which you must free when you're done with it.

     The caller may pass nsnull for charset and it will be interpreted as "us-ascii".

       Caller must call PL_strfree on fullAddress
	 */
	NS_IMETHOD MakeFullAddress (const char *charset, const char* name, const char* addr, char ** fullAddress) = 0;

	/* MSG_ParseRFC822Addresses returns quoted parsable addresses
	   This function removes the quoting if you want to show the
	   names to users. e.g. summary file, address book
	 */
	NS_IMETHOD UnquotePhraseOrAddr (const char *charset, const char *line, char** lineout) = 0;

}; 

#endif /* nsIMSGRFC822PARSER_h__ */

