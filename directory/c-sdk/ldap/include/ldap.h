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

/* ldap.h - general header file for libldap */

#ifndef _LDAP_H
#define _LDAP_H

/* Standard LDAP API functions and declarations */ 
#include "ldap-standard.h"

/* Extensions to the LDAP standard */
#include "ldap-extension.h"

/* A deprecated API is an API that we recommend you no longer use,
 * due to improvements in the LDAP C SDK. While deprecated APIs are
 * currently still implemented, they may be removed in future
 * implementations, and we recommend using other APIs.
 */

/* Soon-to-be deprecated functions and declarations */
#include "ldap-to-be-deprecated.h"

/* Deprecated functions and declarations */
#include "ldap-deprecated.h"

#endif /* _LDAP_H */

