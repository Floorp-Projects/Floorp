/*
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

/*
 * Common definitions for ldap example programs.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ldap.h>

/*
 * Host name of LDAP server
 */
#define MY_HOST		"localhost"

/*
 * Port number where LDAP server is running
 */
#define	MY_PORT		LDAP_PORT

/*
 * Port number where LDAPS server is running
 */
#define	MY_SSL_PORT		LDAPS_PORT

/*
 * DN of directory manager entry.  This entry should have write access to
 * the entire directory.
 */
#define MGR_DN		"cn=Directory Manager"

/*
 * Password for manager DN.
 */
#define MGR_PW		"secret99"

/*
 * Subtree to search
 */
#define	MY_SEARCHBASE	"dc=example,dc=com"

/*
 * Place where people entries are stored
 */
#define PEOPLE_BASE	"ou=People, " MY_SEARCHBASE

/*
 * DN of a user entry.  This entry does not need any special access to the
 * directory (it is not used to perform modifies, for example).
 */
#define USER_DN		"uid=scarter, " PEOPLE_BASE

/*
 * Password of the user entry.
 */
#define USER_PW		"sprain"

/* 
 * Filter to use when searching.  This one searches for all entries with the
 * surname (last name) of "Jensen".
 */
#define	MY_FILTER	"(sn=Jensen)"

/*
 * Entry to retrieve
 */
#define ENTRYDN "uid=bjensen, " PEOPLE_BASE

/*
 * Password for Babs' entry
 */
#define ENTRYPW "hifalutin"

/*
 * Name of file containing filters
 */
#define MY_FILTERFILE   "xmplflt.conf"
 
/*
 * Tag to use when retrieveing filters
 */
#define MY_FILTERTAG    "ldap-example"

