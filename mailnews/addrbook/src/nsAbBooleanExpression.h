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

#ifndef nsAbBooleanExpression_h__
#define nsAbBooleanExpression_h__

#include "nsIAbBooleanExpression.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsAbBooleanConditionString : public nsIAbBooleanConditionString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABBOOLEANCONDITIONSTRING

    nsAbBooleanConditionString();
    virtual ~nsAbBooleanConditionString();

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsAbBooleanConditionType mCondition;
    nsCString mName;
    nsString mValue;
};

class nsAbBooleanExpression: public nsIAbBooleanExpression
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABBOOLEANEXPRESSION

    nsAbBooleanExpression();
    virtual ~nsAbBooleanExpression();

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
protected:
    nsAbBooleanOperationType mOperation;
    nsCOMPtr<nsISupportsArray> mExpressions;
};

nsresult
NS_NewIAbBooleanConditionString(nsIAbBooleanConditionString** aInstancePtrResult);

nsresult
NS_NewIAbBooleanExpression(nsIAbBooleanExpression** aInstancePtrResult);

#endif
