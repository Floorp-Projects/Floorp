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
