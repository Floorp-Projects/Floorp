/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc.  Portions created by Sun are
 * Copyright (C) 2001 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
 *
 * Contributor(s): 
 */

#include "nsAbLDAPProperties.h"

#include "nsAbUtils.h"

#include "nsCOMPtr.h"
#include "nsString.h"

/*
    Table defining the relationship between
    mozilla card properties and corresponding
    ldap properties.

    Multiple entries for a mozilla property define
    there is a many to one relationship between
    ldap properties and the mozilla counterpart.

    There is a one to one relationship between
    a mozilla property and the ldap counterpart.
    If there are multiple entries for a mozilla
    property the first takes precedence.

    This ensures that
    
        1) Generality is maintained when mapping from
           ldap properties to mozilla.
        2) Consistent round tripping when editing 
           mozilla properties which are stored on
           an ldap server

    Ldap properties were obtained from existing
    mozilla code that imported from ldif files,
    comments above each table row indicate which
    class of ldap object the property belongs.
*/

MozillaLdapPropertyRelation mozillaLdapPropertyTable[] =
{
    // inetOrgPerson
    {MozillaProperty_String, "FirstName",        "givenname"},
    // person
    {MozillaProperty_String, "LastName",        "sn"},
    // person
    {MozillaProperty_String, "LastName",        "surname"},
    // person
    {MozillaProperty_String, "DisplayName",        "cn"},
    // person
    {MozillaProperty_String, "DisplayName",        "commonname"},
    // inetOrfPerson
    {MozillaProperty_String, "DisplayName",        "displayname"},
    // mozilla specific
    {MozillaProperty_String, "NickName",        "xmozillanickname"},
    // inetOrfPerson
    {MozillaProperty_String, "PrimaryEmail",    "mail"},
    // mozilla specific
    {MozillaProperty_String, "SecondEmail",        "xmozillasecondemail"},
    // person
    {MozillaProperty_String, "WorkPhone",        "telephonenumber"},
    // inetOrgPerson
    {MozillaProperty_String, "HomePhone",        "homephone"},
    // ?
    {MozillaProperty_String, "FaxNumber",        "fax"},
    // organizationalPerson
    {MozillaProperty_String, "FaxNumber",        "facsimiletelephonenumber"},
    // inetOrgPerson
    {MozillaProperty_String, "PagerNumber",        "pager"},
    // ?
    {MozillaProperty_String, "PagerNumber",        "pagerphone"},
    // inetOrgPerson
    {MozillaProperty_String, "CellularNumber",    "mobile"},
    // ?
    {MozillaProperty_String, "CellularNumber",    "cellphone"},
    // ?
    {MozillaProperty_String, "CellularNumber",    "carphone"},

    // No Home* properties defined yet

    // organizationalPerson
    {MozillaProperty_String, "WorkAddress",        "postofficebox"},
    // ?
    {MozillaProperty_String, "WorkAddress",        "streetaddress"},
    // ?
    {MozillaProperty_String, "WorkCity",        "l"},
    // ?
    {MozillaProperty_String, "WorkCity",        "locality"},
    // ?
    {MozillaProperty_String, "WorkState",        "st"},
    // ?
    {MozillaProperty_String, "WorkState",        "region"},
    // organizationalPerson
    {MozillaProperty_String, "WorkZipCode",        "postalcode"},
    // ?
    {MozillaProperty_String, "WorkZipCode",        "zip"},
    // ?
    {MozillaProperty_String, "WorkCountry",        "countryname"},

    // organizationalPerson
    {MozillaProperty_String, "JobTitle",        "title"},
    // ?
    {MozillaProperty_String, "Department",        "ou"},
    // ?
    {MozillaProperty_String, "Department",        "orgunit"},
    // ?
    {MozillaProperty_String, "Department",        "department"},
    // ?
    {MozillaProperty_String, "Department",        "departmentnumber"},
    // inetOrgPerson
    {MozillaProperty_String, "Company",        "o"},
    // ?
    {MozillaProperty_String, "Company",        "company"},
    // ?
    {MozillaProperty_String, "WorkCountry",        "countryname"},

    // ?
    {MozillaProperty_String, "WebPage1",        "homeurl"},
    // ?
    {MozillaProperty_String, "WebPage2",        "workurl"},

    // ?
    {MozillaProperty_String, "BirthYear",        "birthyear"},

    // ?
    {MozillaProperty_String, "Custom1",        "custom1"},
    // ?
    {MozillaProperty_String, "Custom2",        "custom2"},
    // ?
    {MozillaProperty_String, "Custom3",        "custom3"},
    // ?
    {MozillaProperty_String, "Custom4",        "custom4"},

    // ?
    {MozillaProperty_String, "Notes",        "notes"},
    // person
    {MozillaProperty_String, "Notes",        "description"},

    // mozilla specfic
    {MozillaProperty_Boolean, "SendPlainText",    "xmozillausehtmlmail"},
    // ?
    {MozillaProperty_Int, "LastModifiedDate",    "modifytimestamp"}
};

const MozillaLdapPropertyRelation* MozillaLdapPropertyRelator::table = mozillaLdapPropertyTable;
const int MozillaLdapPropertyRelator::tableSize = sizeof (mozillaLdapPropertyTable) / sizeof (MozillaLdapPropertyRelation);

PRBool MozillaLdapPropertyRelator::IsInitialized = PR_FALSE ;
nsHashtable MozillaLdapPropertyRelator::mMozillaToLdap;
nsHashtable MozillaLdapPropertyRelator::mLdapToMozilla;

void MozillaLdapPropertyRelator::Initialize(void)
{
    if (IsInitialized) { return ; }

    for (int i = tableSize - 1 ; i >= 0 ; -- i) {
        nsCStringKey keyMozilla (table [i].mozillaProperty, -1, nsCStringKey::NEVER_OWN);
        nsCStringKey keyLdap (table [i].ldapProperty, -1, nsCStringKey::NEVER_OWN);

        mLdapToMozilla.Put(&keyLdap, NS_REINTERPRET_CAST(void *, NS_CONST_CAST(MozillaLdapPropertyRelation*, &table[i]))) ;
        mMozillaToLdap.Put(&keyMozilla, NS_REINTERPRET_CAST(void *, NS_CONST_CAST(MozillaLdapPropertyRelation*, &table[i]))) ;
    }
    IsInitialized = PR_TRUE;
}

const MozillaLdapPropertyRelation* MozillaLdapPropertyRelator::findMozillaPropertyFromLdap (const char* ldapProperty)
{
    Initialize();
    nsCStringKey key (ldapProperty) ;

    return NS_REINTERPRET_CAST(const MozillaLdapPropertyRelation *, mLdapToMozilla.Get(&key)) ;

}

const MozillaLdapPropertyRelation* MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (const char* mozillaProperty)
{
    Initialize();
    nsCStringKey key (mozillaProperty) ;

    return NS_REINTERPRET_CAST(const MozillaLdapPropertyRelation *, mMozillaToLdap.Get(&key)) ;

}

nsresult MozillaLdapPropertyRelator::createCardPropertyFromLDAPMessage (nsILDAPMessage* message,
        nsIAbCard* card,
        PRBool* hasSetCardProperty)
{
    nsresult rv = NS_OK;
    CharPtrArrayGuard attrs;

    rv = message->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    *hasSetCardProperty = PR_FALSE;
    for (PRUint32 i = 0; i < attrs.GetSize(); i++)
    {
        const MozillaLdapPropertyRelation* property = findMozillaPropertyFromLdap (attrs[i]);
        if (!property)
            continue;

        PRUnicharPtrArrayGuard vals;
        rv = message->GetValues(attrs.GetArray()[i], vals.GetSizeAddr(), vals.GetArrayAddr());
        if (NS_FAILED(rv))
            continue;

        if (vals.GetSize())
        {
            rv = card->SetCardValue (property->mozillaProperty, vals[0]);
            if (NS_SUCCEEDED(rv))
                *hasSetCardProperty = PR_TRUE;
            else
            {
                rv = NS_OK;
            }
        }
    }

    return rv;
}

