/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
/*
 * This interface is implemented by libmime. This interface is used by 
 * a Content-Type handler "Plug In" (i.e. vCard) for accessing various 
 * internal information about the object class system of libmime. When 
 * libmime progresses to a C++ object class, this would probably change.
 */
#ifndef nsMimeAddress_h_
#define nsMimeAddress_h_

#include "prtypes.h"

int
ParseRFC822Addresses (const char *line,
						              char **names,
						              char **addresses);

/* Given a name or address that might have been quoted
 it will take out the escape and double quotes
 The caller is responsible for freeing the resulting
 string.
 */
int
UnquotePhraseOrAddr (char *line, char** lineout);

/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `mailbox' portions.
 */
char *
ExtractRFC822AddressMailboxes (const char *line);

/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `user name' portions.  If any of
   the addresses doesn't have a name, then the mailbox is used instead.

   The names are *unquoted* and therefore cannot be re-parsed in any way.
   They are, however, nice and human-readable.
 */
char *
ExtractRFC822AddressNames (const char *line);

/* Like ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
char *
ExtractRFC822AddressName (const char *line);

/* Given a string which contains a list of RFC822 addresses, returns a new
   string with the same data, but inserts missing commas, parses and reformats
   it, and wraps long lines with newline-tab.
 */
char *
ReformatRFC822Addresses (const char *line);

#endif /* nsMimeAddress_h_ */
