/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "plstr.h"
#include "prmem.h"

#include "nsMsgBaseCID.h"
#include "nsIMsgHeaderParser.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 

int
ParseRFC822Addresses (const char *line,
						              char **names,
						              char **addresses)
{
  PRUint32            numAddresses;
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
  {
    pHeader->ParseHeaderAddresses(NULL, line, names, addresses, &numAddresses);
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
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
  {
    pHeader->UnquotePhraseOrAddr(NULL, line, lineout);
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
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
    pHeader->ExtractHeaderAddressMailboxes(NULL, line, &retVal);

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
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
    pHeader->ExtractHeaderAddressNames(NULL, line, &retVal);

  return retVal;
}

/* Like ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
char *
ExtractRFC822AddressName (const char *line)
{
  char                *retVal = NULL;
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
    pHeader->ExtractHeaderAddressName(NULL, line, &retVal);

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
  nsCOMPtr<nsIMsgHeaderParser>  pHeader;

  nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                                    (void **) getter_AddRefs(pHeader)); 
  if (NS_SUCCEEDED(res) && pHeader)
    pHeader->ReformatHeaderAddresses(NULL, line, &retVal);

  return retVal;
}
