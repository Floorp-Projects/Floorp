/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Created by: Paul Sandoz   <paul.sandoz@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAbBoolExprToLDAPFilter.h"
#include "nsAbLDAPProperties.h"
#include "nsXPIDLString.h"

const int nsAbBoolExprToLDAPFilter::TRANSLATE_CARD_PROPERTY = 1 << 0 ;
const int nsAbBoolExprToLDAPFilter::ALLOW_NON_CONVERTABLE_CARD_PROPERTY = 1 << 1 ;

nsresult nsAbBoolExprToLDAPFilter::Convert (
    nsIAbBooleanExpression* expression,
    nsCString& filter,
    int flags)
{
    nsresult rv;

    nsCString f;
    rv = FilterExpression (expression, f, flags);
    NS_ENSURE_SUCCESS(rv, rv);

    filter = f;
    return rv;
}

nsresult nsAbBoolExprToLDAPFilter::FilterExpression (
    nsIAbBooleanExpression* expression,
    nsCString& filter,
    int flags)
{
    nsresult rv;

    nsAbBooleanOperationType operation;
    rv = expression->GetOperation (&operation);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsArray> childExpressions;
    rv = expression->GetExpressions (getter_AddRefs (childExpressions));
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRUint32 count;
    rv = childExpressions->Count (&count);
    NS_ENSURE_SUCCESS(rv, rv);

    if (count == 0)
        return NS_OK;

    /*
     * 3rd party query integration with Mozilla is achieved 
     * by calling nsAbLDAPDirectoryQuery::DoQuery(). Thus
     * we can arrive here with a query asking for all the
     * ldap attributes using the card:nsIAbCard interface.
     *
     * So we need to check that we are not creating a condition 
     * filter against this expression otherwise we will end up with an invalid 
     * filter equal to "(|)".
    */
    
    if (count == 1 )
    {
        nsCOMPtr<nsISupports> item;
        rv = childExpressions->GetElementAt (0, getter_AddRefs (item));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAbBooleanConditionString> childCondition(do_QueryInterface(item, &rv));
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLCString name;
            rv = childCondition->GetName (getter_Copies (name));
            NS_ENSURE_SUCCESS(rv, rv);

            if(name.Equals("card:nsIAbCard"))
                return NS_OK;
        }
    }

    filter.AppendLiteral("(");
    switch (operation)
    {
        case nsIAbBooleanOperationTypes::AND:
            filter.AppendLiteral("&");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        case nsIAbBooleanOperationTypes::OR:
            filter.AppendLiteral("|");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        case nsIAbBooleanOperationTypes::NOT:
            if (count > 1)
                return NS_ERROR_FAILURE;
            filter.AppendLiteral("!");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        default:
            break;
    }
    filter += NS_LITERAL_CSTRING(")");

    return rv;
}

nsresult nsAbBoolExprToLDAPFilter::FilterExpressions (
    nsISupportsArray* expressions,
    nsCString& filter,
    int flags)
{
    nsresult rv;

    PRUint32 count;
    rv = expressions->Count (&count);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < count; i++)
    {
        nsCOMPtr<nsISupports> item;
        rv = expressions->GetElementAt (i, getter_AddRefs (item));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAbBooleanConditionString> childCondition(do_QueryInterface(item, &rv));
        if (NS_SUCCEEDED(rv))
        {
            rv = FilterCondition (childCondition, filter, flags);
            NS_ENSURE_SUCCESS(rv, rv);
            continue;
        }

        nsCOMPtr<nsIAbBooleanExpression> childExpression(do_QueryInterface(item, &rv));
        if (NS_SUCCEEDED(rv))
        {
            rv = FilterExpression (childExpression, filter, flags);
            NS_ENSURE_SUCCESS(rv, rv);
            continue;
        }
    }

    return rv;
}

nsresult nsAbBoolExprToLDAPFilter::FilterCondition (
    nsIAbBooleanConditionString* condition,
    nsCString& filter,
    int flags)
{
    nsresult rv;

    nsAbBooleanConditionType conditionType;
    rv = condition->GetCondition (&conditionType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString name;
    rv = condition->GetName (getter_Copies (name));
    NS_ENSURE_SUCCESS(rv, rv);

    const char* ldapProperty = name.get ();
    if (flags & TRANSLATE_CARD_PROPERTY)
    {
        const MozillaLdapPropertyRelation* p =
            MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (name.get ());
        if (p)
            ldapProperty = p->ldapProperty;
        else if (!(flags & ALLOW_NON_CONVERTABLE_CARD_PROPERTY))
            return NS_OK;
    }

    nsXPIDLString value;
    rv = condition->GetValue (getter_Copies (value));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ConvertUCS2toUTF8 vUTF8 (value);

    switch (conditionType)
    {
        case nsIAbBooleanConditionTypes::DoesNotExist:
            filter += NS_LITERAL_CSTRING("(!(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=*))");
            break;
        case nsIAbBooleanConditionTypes::Exists:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=*)");
            break;
        case nsIAbBooleanConditionTypes::Contains:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=*") +
                      vUTF8 +
                      NS_LITERAL_CSTRING("*)");
            break;
        case nsIAbBooleanConditionTypes::DoesNotContain:
            filter += NS_LITERAL_CSTRING("(!(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=*") +
                      vUTF8 +
                      NS_LITERAL_CSTRING("*))");
            break;
        case nsIAbBooleanConditionTypes::Is:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING(")");
            break;
        case nsIAbBooleanConditionTypes::IsNot:
            filter += NS_LITERAL_CSTRING("(!(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING("))");
            break;
        case nsIAbBooleanConditionTypes::BeginsWith:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING("*)");
            break;
        case nsIAbBooleanConditionTypes::EndsWith:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("=*") +
                      vUTF8 +
                      NS_LITERAL_CSTRING(")");
            break;
        case nsIAbBooleanConditionTypes::LessThan:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("<=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING(")");
            break;
        case nsIAbBooleanConditionTypes::GreaterThan:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING(">=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING(")");
            break;
        case nsIAbBooleanConditionTypes::SoundsLike:
            filter += NS_LITERAL_CSTRING("(") +
                      nsDependentCString(ldapProperty) +
                      NS_LITERAL_CSTRING("~=") +
                      vUTF8 +
                      NS_LITERAL_CSTRING(")");
            break;
        case nsIAbBooleanConditionTypes::RegExp:
            break;
        default:
            break;
    }

    return rv;
}

