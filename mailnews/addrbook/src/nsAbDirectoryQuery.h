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

#ifndef nsAbDirectoryQuery_h__
#define nsAbDirectoryQuery_h__

#include "nsIAbDirectoryQuery.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsAbDirectoryQuerySimpleBooleanExpression : public nsIAbBooleanExpression
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABBOOLEANEXPRESSION

    nsAbDirectoryQuerySimpleBooleanExpression();
    virtual ~nsAbDirectoryQuerySimpleBooleanExpression();

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

public:
    nsCOMPtr<nsISupportsArray> mExpressions;
    nsAbBooleanOperationType mOperation;
};


class nsAbDirectoryQueryArguments : public nsIAbDirectoryQueryArguments
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABDIRECTORYQUERYARGUMENTS

    nsAbDirectoryQueryArguments();
    virtual ~nsAbDirectoryQueryArguments();

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsCOMPtr<nsISupports> mExpression;
    PRBool mQuerySubDirectories;
    nsCStringArray mReturnProperties;
};


class nsAbDirectoryQueryPropertyValue : public nsIAbDirectoryQueryPropertyValue
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABDIRECTORYQUERYPROPERTYVALUE

    nsAbDirectoryQueryPropertyValue();
    nsAbDirectoryQueryPropertyValue(const char* aName,
          const PRUnichar* aValue);
    nsAbDirectoryQueryPropertyValue(const char* aName,
          nsISupports* aValueISupports);
    virtual ~nsAbDirectoryQueryPropertyValue();

protected:
    nsCString mName;
    nsString mValue;
    nsCOMPtr<nsISupports> mValueISupports;
};


class nsAbDirectoryQueryResult : public nsIAbDirectoryQueryResult
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABDIRECTORYQUERYRESULT

    nsAbDirectoryQueryResult();
    nsAbDirectoryQueryResult(PRInt32 aContextID,
          nsIAbDirectoryQueryArguments* aContextArgs,
          PRInt32 aType,
          nsCOMPtr<nsISupportsArray> aResult);
    virtual ~nsAbDirectoryQueryResult();

protected:
    PRInt32 mContextID;
    nsCOMPtr<nsIAbDirectoryQueryArguments> mContextArgs;
    PRInt32 mType;
    nsCOMPtr<nsISupportsArray> mResult;
};




#include "nsIAbDirectory.h"

class nsAbDirectoryQuery : public nsIAbDirectoryQuery
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABDIRECTORYQUERY

    nsAbDirectoryQuery(nsIAbDirectory* aDirectory);
    virtual ~nsAbDirectoryQuery();

protected:
    nsresult query (nsIAbDirectory* directory,
        nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32* resultLimit);
    nsresult queryChildren (nsIAbDirectory* directory,
        nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32* resultLimit);
    nsresult queryCards (nsIAbDirectory* directory,
        nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32* resultLimit);
    nsresult matchCard (nsIAbCard* card,
        nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32* resultLimit);
    nsresult matchCardExpression (nsIAbCard* card,
        nsIAbBooleanExpression* expression,
        PRBool* result);
    nsresult matchCardCondition (nsIAbCard* card,
        nsIAbBooleanConditionString* condition,
        PRBool* matchFound);

    nsresult queryMatch (nsIAbCard* card,
        nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener);
    nsresult queryFinished (nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener);
    nsresult queryError (nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener);
    
protected:
    nsCOMPtr<nsIAbDirectory> mDirectory;
};

nsresult
NS_NewAbDirectoryQuerySimpleBooleanExpression(nsIAbBooleanExpression** aInstancePtrResult);

nsresult
NS_NewIAbDirectoryQueryArguments(nsIAbDirectoryQueryArguments** aInstancePtrResult);


#endif
