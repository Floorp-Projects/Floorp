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

    filter.Append ("(");
    switch (operation)
    {
        case nsIAbBooleanOperationTypes::AND:
            filter.Append ("&");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        case nsIAbBooleanOperationTypes::OR:
            filter.Append ("|");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        case nsIAbBooleanOperationTypes::NOT:
            if (count > 1)
                return NS_ERROR_FAILURE;
            filter.Append ("!");
            rv = FilterExpressions (childExpressions, filter, flags);
            break;
        default:
            break;
    }
    filter.Append (")");

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
            filter.Append ("(!(");
            filter.Append (ldapProperty);
            filter.Append ("=*");
            filter.Append ("))");
            break;
        case nsIAbBooleanConditionTypes::Exists:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("=*");
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::Contains:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append ("*");
            filter.Append (vUTF8);
            filter.Append ("*");
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::DoesNotContain:
            filter.Append ("(!(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append ("*");
            filter.Append (vUTF8);
            filter.Append ("*");
            filter.Append ("))");
            break;
        case nsIAbBooleanConditionTypes::Is:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append (vUTF8);
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::IsNot:
            filter.Append ("(!(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append (vUTF8);
            filter.Append ("))");
            break;
        case nsIAbBooleanConditionTypes::BeginsWith:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append (vUTF8);
            filter.Append ("*");
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::EndsWith:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("=");
            filter.Append ("*");
            filter.Append (vUTF8);
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::LessThan:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("<=");
            filter.Append (vUTF8);
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::GreaterThan:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append (">=");
            filter.Append (vUTF8);
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::SoundsLike:
            filter.Append ("(");
            filter.Append (ldapProperty);
            filter.Append ("~=");
            filter.Append (vUTF8);
            filter.Append (")");
            break;
        case nsIAbBooleanConditionTypes::RegExp:
            break;
        default:
            break;
    }

    return rv;
}

