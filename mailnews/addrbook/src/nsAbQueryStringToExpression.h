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

#ifndef nsAbQueryStringToExpression_h__
#define nsAbQueryStringToExpression_h__

#include "nsIAbBooleanExpression.h"

class nsAbQueryStringToExpression
{
public:
    static nsresult Convert (
        const char* queryString,
        nsIAbBooleanExpression** expression);

protected:
    static nsresult ParseExpression (
        const char** index,
        nsISupports** expression);
    static nsresult ParseExpressions (
        const char** index,
        nsIAbBooleanExpression* expression);
    static nsresult ParseCondition (
        const char** index,
        const char* indexBracketClose,
        nsIAbBooleanConditionString** conditionString);

    static nsresult ParseConditionEntry (
        const char** index,
        const char* indexBracketClose,
        char** entry);
    static nsresult ParseOperationEntry (
        const char* indexBracketOpen1,
        const char* indexBracketOpen2,
        char** operation);

    static nsresult CreateBooleanExpression(
        const char* operation,
        nsIAbBooleanExpression** expression);
    static nsresult CreateBooleanConditionString (
        const char* attribute,
        const char* condition,
        const char* value,
        nsIAbBooleanConditionString** conditionString);
};

#endif
