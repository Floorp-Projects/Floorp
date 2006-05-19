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
#define NEEDPROTOS
#define NEEDGETOPT
#define FILTERFILE	"ldapfilter.conf"
#define TEMPLATEFILE	"ldaptemplates.conf"

#define MOZILLA_CLIENT 1
#define NET_SSL 1

#ifdef __powerc
#define SUPPORT_OPENTRANSPORT	1
#define OTUNIXERRORS 1
#endif

#ifndef macintosh
#define macintosh
#endif 

#define XP_MAC
#define DEBUG	1

#include "IDE_Options.h"
// #define NO_USERINTERFACE
// #define LDAP_DEBUG
