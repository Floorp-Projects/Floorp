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

#include "nsAbQueryStringToExpression.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsITextToSubURI.h"
#include "nsAbBooleanExpression.h"


nsresult nsAbQueryStringToExpression::Convert (
    const char* queryString,
    nsIAbBooleanExpression** expression)
{
    nsresult rv;

    nsCAutoString q(queryString);
    q.StripWhitespace ();
    queryString = q.get ();

    nsCOMPtr<nsISupports> s;
    rv = ParseExpression (&queryString, getter_AddRefs(s));
    NS_ENSURE_SUCCESS(rv, rv);

    // Case: Not end of string
    if (*queryString != 0)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIAbBooleanExpression> e(do_QueryInterface(s, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    *expression = e;
    NS_IF_ADDREF(*expression);

    return rv;
}

nsresult nsAbQueryStringToExpression::ParseExpression (
    const char** index,
    nsISupports** expression)
{
    nsresult rv;

    if (**index != '(')
        return NS_ERROR_FAILURE;

    const char* indexBracket = *index + 1;
    while (*indexBracket &&
        *indexBracket != '(' && *indexBracket != ')')
        indexBracket++;

    // Case: End of string
    if (*indexBracket == 0)
        return NS_ERROR_FAILURE;

    // Case: "((" or "()"
    if (indexBracket == *index + 1)
    {
         return NS_ERROR_FAILURE;
    }
    // Case: "(*("
    else if (*indexBracket == '(')
    {
        // printf ("Case: (*(: %s\n", *index);

        nsXPIDLCString operation;
        rv = ParseOperationEntry (
            *index, indexBracket,
            getter_Copies (operation));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAbBooleanExpression> e;
        rv = CreateBooleanExpression(operation,
            getter_AddRefs(e));
        NS_ENSURE_SUCCESS(rv, rv);

        // Case: "(*)(*)....(*))"
        *index = indexBracket;
        rv = ParseExpressions (index, e);
        NS_ENSURE_SUCCESS(rv, rv);

        *expression = e;
        NS_IF_ADDREF(*expression);
    }
    // Case" "(*)"
    else if (*indexBracket == ')')
    {
        // printf ("Case: (*): %s\n", *index);

        nsCOMPtr<nsIAbBooleanConditionString> conditionString;
        rv = ParseCondition (index, indexBracket,
            getter_AddRefs(conditionString));
        NS_ENSURE_SUCCESS(rv, rv);

        *expression = conditionString;
        NS_IF_ADDREF(*expression);
    }

    if (**index != ')')
        return NS_ERROR_FAILURE;

    (*index)++;

    return NS_OK;
}


nsresult nsAbQueryStringToExpression::ParseExpressions (
    const char** index,
    nsIAbBooleanExpression* expression)
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> expressions;
    NS_NewISupportsArray(getter_AddRefs(expressions));

    // Case: ")(*)(*)....(*))"
    // printf ("Case: )(*)(*)....(*)): %s\n", *index);
    while (**index == '(')
    {
        nsCOMPtr<nsISupports> childExpression;
        rv = ParseExpression (index, getter_AddRefs (childExpression));
        NS_ENSURE_SUCCESS(rv, rv);

        expressions->AppendElement (childExpression);
    }

    if (**index == 0)
        return NS_ERROR_FAILURE;

    // Case: "))"
    // printf ("Case: )): %s\n", *index);

    if (**index != ')')
        return NS_ERROR_FAILURE;

    expression->SetExpressions (expressions);

    return NS_OK;
}

nsresult nsAbQueryStringToExpression::ParseCondition (
    const char** index,
    const char* indexBracketClose,
    nsIAbBooleanConditionString** conditionString)
{
    nsresult rv;

    (*index)++;

    nsXPIDLCString entries[3];
    for (int i = 0; i < 3; i++)
    {
        rv = ParseConditionEntry (index, indexBracketClose,
                getter_Copies (entries[i]));
        NS_ENSURE_SUCCESS(rv, rv);

        if (*index == indexBracketClose)
            break;
    }
    
    if (*index != indexBracketClose)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIAbBooleanConditionString> c;
    rv = CreateBooleanConditionString (
        entries[0],
        entries[1],
        entries[2],
        getter_AddRefs (c));
    NS_ENSURE_SUCCESS(rv, rv);

    *conditionString = c;
    NS_IF_ADDREF(*conditionString);

    return NS_OK;
}

nsresult nsAbQueryStringToExpression::ParseConditionEntry (
    const char** index,
    const char* indexBracketClose,
    char** entry)
{
    const char* indexDeliminator = *index;
    while (indexDeliminator != indexBracketClose &&
        *indexDeliminator != ',')
        indexDeliminator++;

    int entryLength = indexDeliminator - *index;
    if (entryLength)
        *entry = nsCRT::strndup (*index, entryLength); 
    else
        *entry = 0;

    if (indexDeliminator != indexBracketClose)
        *index = indexDeliminator + 1;
    else
        *index = indexDeliminator;

    return NS_OK;
}

nsresult nsAbQueryStringToExpression::ParseOperationEntry (
    const char* indexBracketOpen1,
    const char* indexBracketOpen2,
    char** operation)
{
    int operationLength = indexBracketOpen2 - indexBracketOpen1 - 1;
    if (operationLength)
        *operation = nsCRT::strndup (indexBracketOpen1 + 1,
            operationLength); 
    else
        *operation = 0;

    return NS_OK;
}

nsresult nsAbQueryStringToExpression::CreateBooleanExpression(
        const char* operation,
        nsIAbBooleanExpression** expression)
{
    nsAbBooleanOperationType op;
    if (nsCRT::strcasecmp (operation, "and") == 0)
        op = nsIAbBooleanOperationTypes::AND;
    else if (nsCRT::strcasecmp (operation, "or") == 0)
        op = nsIAbBooleanOperationTypes::OR;
    else if (nsCRT::strcasecmp (operation, "not") == 0)
        op = nsIAbBooleanOperationTypes::NOT;
    else
        return NS_ERROR_FAILURE;

    nsresult rv;

           rv = NS_NewIAbBooleanExpression(expression);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = (*expression)->SetOperation (op);

    return rv;
}

nsresult nsAbQueryStringToExpression::CreateBooleanConditionString (
    const char* attribute,
    const char* condition,
    const char* value,
    nsIAbBooleanConditionString** conditionString)
{
    if (attribute == 0 || condition == 0 || value == 0)
        return NS_ERROR_FAILURE;

    nsAbBooleanConditionType c;

    if (nsCRT::strcasecmp (condition, "=") == 0)
        c = nsIAbBooleanConditionTypes::Is;
    else if (nsCRT::strcasecmp (condition, "!=") == 0)
        c = nsIAbBooleanConditionTypes::IsNot;
    else if (nsCRT::strcasecmp (condition, "lt") == 0)
        c = nsIAbBooleanConditionTypes::LessThan;
    else if (nsCRT::strcasecmp (condition, "gt") == 0)
        c = nsIAbBooleanConditionTypes::GreaterThan;
    else if (nsCRT::strcasecmp (condition, "bw") == 0)
        c = nsIAbBooleanConditionTypes::BeginsWith;
    else if (nsCRT::strcasecmp (condition, "ew") == 0)
        c = nsIAbBooleanConditionTypes::EndsWith;
    else if (nsCRT::strcasecmp (condition, "c")== 0)
        c = nsIAbBooleanConditionTypes::Contains;
    else if (nsCRT::strcasecmp (condition, "!c") == 0)
        c = nsIAbBooleanConditionTypes::DoesNotContain;
    else if (nsCRT::strcasecmp (condition, "~=") == 0)
        c = nsIAbBooleanConditionTypes::SoundsLike;
    else if (nsCRT::strcasecmp (condition, "regex") == 0)
        c = nsIAbBooleanConditionTypes::RegExp;
    else
        return NS_ERROR_FAILURE;

    nsresult rv;

    nsCOMPtr<nsIAbBooleanConditionString> cs;
    rv = NS_NewIAbBooleanConditionString (getter_AddRefs(cs));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = cs->SetCondition (c);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID,&rv); 
    if (NS_SUCCEEDED(rv))
    {
        nsXPIDLString attributeUCS2;
        nsXPIDLString valueUCS2;

        rv = textToSubURI->UnEscapeAndConvert("UTF-8",
            attribute, getter_Copies(attributeUCS2));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = textToSubURI->UnEscapeAndConvert("UTF-8",
            value, getter_Copies(valueUCS2));
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ConvertUCS2toUTF8 attributeUTF8(attributeUCS2);

        rv = cs->SetName (attributeUTF8.get ());
        NS_ENSURE_SUCCESS(rv, rv);
        rv = cs->SetValue (valueUCS2);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
        NS_ConvertUTF8toUCS2 valueUCS2(value);

        rv = cs->SetName (attribute);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = cs->SetValue (valueUCS2.get ());
        NS_ENSURE_SUCCESS(rv, rv);
    }
            

    *conditionString = cs;
    NS_IF_ADDREF(*conditionString);

    return NS_OK;
}


