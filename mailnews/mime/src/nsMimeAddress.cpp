/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "plstr.h"
#include "prmem.h"

#include "nsMsgBaseCID.h"
#include "nsIMsgHeaderParser.h"

static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 

extern int MK_OUT_OF_MEMORY;

int
ParseRFC822Addresses (const char *line,
						              char **names,
						              char **addresses)
{
  PRUint32            numAddresses;
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->ParseHeaderAddresses(NULL, line, names, addresses, numAddresses);
    pHeader->Release();
    return numAddresses;
  }

  return 0;;
}

/* Given a name or address that might have been quoted
 it will take out the escape and double quotes
 The caller is responsible for freeing the resulting
 string.
 */
int
UnquotePhraseOrAddr (char *line, char** lineout)
{
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->UnquotePhraseOrAddr(NULL, line, lineout);
    pHeader->Release();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;;
}

/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `mailbox' portions.
 */
char *
ExtractRFC822AddressMailboxes (const char *line)
{
  char                *retVal = NULL;
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->ExtractHeaderAddressMailboxes(NULL, line, &retVal);
    pHeader->Release();
  }

  return retVal;
}


/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `user name' portions.  If any of
   the addresses doesn't have a name, then the mailbox is used instead.

   The names are *unquoted* and therefore cannot be re-parsed in any way.
   They are, however, nice and human-readable.
 */
char *
ExtractRFC822AddressNames (const char *line)
{
  char                *retVal = NULL;
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->ExtractHeaderAddressNames(NULL, line, &retVal);
    pHeader->Release();
  }

  return retVal;
}

/* Like ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
char *
ExtractRFC822AddressName (const char *line)
{
  char                *retVal = NULL;
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->ExtractHeaderAddressName(NULL, line, &retVal);
    pHeader->Release();
  }

  return retVal;
}

/* Given a string which contains a list of RFC822 addresses, returns a new
   string with the same data, but inserts missing commas, parses and reformats
   it, and wraps long lines with newline-tab.
 */
char *
ReformatRFC822Addresses (const char *line)
{
  char                *retVal = NULL;
  nsIMsgHeaderParser  *pHeader;
  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                   NULL, 
                                                   nsIMsgHeaderParser::GetIID(), 
                                                   (void **) &pHeader);
  if (NS_SUCCEEDED(rv)) 
  {
    pHeader->ReformatHeaderAddresses(NULL, line, &retVal);
    pHeader->Release();
  }

  return retVal;
}
