/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
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

#ifndef nsAbLDAPProperties_h__
#define nsAbLDAPProperties_h__

#include "prio.h"
#include "nsMemory.h"

#include "nsILDAPMessage.h"
#include "nsIAbCard.h"
#include "nsHashtable.h"

#define NS_LDAPCONNECTION_CONTRACTID     "@mozilla.org/network/ldap-connection;1" 
#define NS_LDAPOPERATION_CONTRACTID      "@mozilla.org/network/ldap-operation;1" 
#define NS_LDAPMESSAGE_CONTRACTID      "@mozilla.org/network/ldap-message;1"
#define NS_LDAPURL_CONTRACTID       "@mozilla.org/network/ldap-url;1"

enum MozillaPropertyType
{
    MozillaProperty_String,
    MozillaProperty_Boolean,
    MozillaProperty_Int
};

struct MozillaLdapPropertyRelation
{
    MozillaPropertyType mozillaPropertyType;
    const char* mozillaProperty;
    const char* ldapProperty;
};

class MozillaLdapPropertyRelator
{
public:
    static const MozillaLdapPropertyRelation* table;
    static const int tableSize;
    static nsHashtable mLdapToMozilla ;
    static nsHashtable mMozillaToLdap ;
    static PRBool IsInitialized;

public:
    static const MozillaLdapPropertyRelation* findMozillaPropertyFromLdap (const char* ldapProperty);
    static const MozillaLdapPropertyRelation* findLdapPropertyFromMozilla (const char* mozillaProperty);
    static void Initialize(void);

    static nsresult createCardPropertyFromLDAPMessage (nsILDAPMessage* message,
            nsIAbCard* card,
            PRBool* hasSetCardProperty);
};

#endif
