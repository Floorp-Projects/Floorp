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

#ifndef nsBooleanExpressionToLDAPFilter_h__
#define nsBooleanExpressionToLDAPFilter_h__

#include "nsIAbBooleanExpression.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsAbBoolExprToLDAPFilter
{
public:
    static const int TRANSLATE_CARD_PROPERTY ;
    static const int ALLOW_NON_CONVERTABLE_CARD_PROPERTY ;

    static nsresult Convert (
            nsIAbBooleanExpression* expression,
            nsCString& filter,
            int flags = TRANSLATE_CARD_PROPERTY);

protected:
    static nsresult FilterExpression (
        nsIAbBooleanExpression* expression,
        nsCString& filter,
        int flags);
    static nsresult FilterExpressions (
        nsISupportsArray* expressions,
        nsCString& filter,
        int flags);
    static nsresult FilterCondition (
        nsIAbBooleanConditionString* condition,
        nsCString& filter,
        int flags);
};

#endif
