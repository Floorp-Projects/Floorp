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

#include "nsAbLDAPDirectoryQuery.h"
#include "nsAbBoolExprToLDAPFilter.h"
#include "nsAbLDAPProperties.h"
#include "nsILDAPErrors.h"
#include "nsILDAPOperation.h"
#include "nsAbUtils.h"

#include "nsXPIDLString.h"
#include "nsAutoLock.h"

class nsAbQueryLDAPMessageListener : public nsILDAPMessageListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPMESSAGELISTENER

    nsAbQueryLDAPMessageListener (
        nsAbLDAPDirectoryQuery* directoryQuery,
        PRInt32 contextID,
        nsILDAPURL* url,
        nsILDAPConnection* connection,
        nsIAbDirectoryQueryArguments* queryArguments,
        nsIAbDirectoryQueryResultListener* queryListener,
        PRInt32 resultLimit = -1,
        PRInt32 timeOut = 0);
    virtual ~nsAbQueryLDAPMessageListener ();

protected:
    nsresult OnLDAPMessageBind (nsILDAPMessage *aMessage);
    nsresult OnLDAPMessageSearchEntry (nsILDAPMessage *aMessage,
            nsIAbDirectoryQueryResult** result);
    nsresult OnLDAPMessageSearchResult (nsILDAPMessage *aMessage,
            nsIAbDirectoryQueryResult** result);

    nsresult QueryResultStatus (nsISupportsArray* properties,
            nsIAbDirectoryQueryResult** result, PRUint32 resultStatus);

protected:
    friend class nsAbLDAPDirectoryQuery;
    nsresult Cancel ();
    nsresult Initiate ();

protected:
    nsAbLDAPDirectoryQuery* mDirectoryQuery;
    PRInt32 mContextID;
    nsCOMPtr<nsILDAPURL> mUrl;
    nsCOMPtr<nsILDAPConnection> mConnection;
    nsCOMPtr<nsIAbDirectoryQueryArguments> mQueryArguments;
    nsCOMPtr<nsIAbDirectoryQueryResultListener> mQueryListener;
    PRInt32 mResultLimit;
    PRInt32 mTimeOut;

    PRBool mBound;
    PRBool mFinished;
    PRBool mInitialized;
    PRBool mCanceled;

    nsCOMPtr<nsILDAPOperation> mSearchOperation;

    PRLock* mLock;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbQueryLDAPMessageListener, nsILDAPMessageListener)

nsAbQueryLDAPMessageListener::nsAbQueryLDAPMessageListener (
        nsAbLDAPDirectoryQuery* directoryQuery,
        PRInt32 contextID,
        nsILDAPURL* url,
        nsILDAPConnection* connection,
        nsIAbDirectoryQueryArguments* queryArguments,
        nsIAbDirectoryQueryResultListener* queryListener,
        PRInt32 resultLimit,
        PRInt32 timeOut) :
    mDirectoryQuery (directoryQuery),
    mContextID (contextID),
    mUrl (url),
    mConnection (connection),
    mQueryArguments (queryArguments),
    mQueryListener (queryListener),
    mResultLimit (resultLimit),
    mTimeOut (timeOut),
    mBound (PR_FALSE),
    mFinished (PR_FALSE),
    mInitialized(PR_FALSE),
    mCanceled (PR_FALSE),
    mLock(0)

{
    NS_INIT_ISUPPORTS();

    NS_ADDREF(mDirectoryQuery);
}

nsAbQueryLDAPMessageListener::~nsAbQueryLDAPMessageListener ()
{
    if (mLock)
        PR_DestroyLock (mLock);

    NS_RELEASE(mDirectoryQuery);
}

nsresult nsAbQueryLDAPMessageListener::Initiate ()
{
    if(mInitialized == PR_TRUE)
        return NS_OK;
                
    mLock = PR_NewLock ();
    if(!mLock)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    mInitialized = PR_TRUE;

    return NS_OK;
}

nsresult nsAbQueryLDAPMessageListener::Cancel ()
{
    nsresult rv;

    rv = Initiate();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoLock lock(mLock);

    if (mFinished == PR_TRUE || mCanceled == PR_TRUE)
        return NS_OK;

    mCanceled = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP nsAbQueryLDAPMessageListener::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    nsresult rv;

    rv = Initiate();
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 messageType;
    rv = aMessage->GetType(&messageType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool cancelOperation = PR_FALSE;

    // Enter lock
    {
        nsAutoLock lock (mLock);

        if (mFinished == PR_TRUE)
            return NS_OK;

        if (messageType == nsILDAPMessage::RES_SEARCH_RESULT)
            mFinished = PR_TRUE;
        else if (mCanceled == PR_TRUE)
        {
            mFinished = PR_TRUE;
            cancelOperation = PR_TRUE;
        }
    }
    // Leave lock

    nsCOMPtr<nsIAbDirectoryQueryResult> queryResult;
    if (cancelOperation == PR_FALSE)
    {
        switch (messageType)
        {
        case nsILDAPMessage::RES_BIND:
            rv = OnLDAPMessageBind (aMessage);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        case nsILDAPMessage::RES_SEARCH_ENTRY:
            rv = OnLDAPMessageSearchEntry (aMessage, getter_AddRefs (queryResult));
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        case nsILDAPMessage::RES_SEARCH_RESULT:
            rv = OnLDAPMessageSearchResult (aMessage, getter_AddRefs (queryResult));
            NS_ENSURE_SUCCESS(rv, rv);
        default:
            break;
        }
    }
    else
    {
        if (mSearchOperation)
            rv = mSearchOperation->Abandon ();

        rv = QueryResultStatus (nsnull, getter_AddRefs (queryResult), nsIAbDirectoryQueryResult::queryResultStopped);
    }

    if (queryResult)
        rv = mQueryListener->OnQueryItem (queryResult);

    return rv;
}

NS_IMETHODIMP nsAbQueryLDAPMessageListener::OnLDAPInit(nsresult aStatus)
{
    nsresult rv;

    // Make sure that the Init() worked properly
    NS_ENSURE_SUCCESS(aStatus, aStatus);

    // Initiate the LDAP operation
    nsCOMPtr<nsILDAPOperation> ldapOperation =
        do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ldapOperation->Init(mConnection, this);
    NS_ENSURE_SUCCESS(rv, rv);

    // Bind
    rv = ldapOperation->SimpleBind(NULL);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageBind (nsILDAPMessage *aMessage)
{
    if (mBound == PR_TRUE)
        return NS_OK;

    nsresult rv;

    mSearchOperation = do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSearchOperation->Init (mConnection, this);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString dn;
    rv = mUrl->GetDn (getter_Copies (dn));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 scope;
    rv = mUrl->GetScope (&scope);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString filter;
    rv = mUrl->GetFilter (getter_Copies (filter));
    NS_ENSURE_SUCCESS(rv, rv);

    CharPtrArrayGuard attributes;
    rv = mUrl->GetAttributes (attributes.GetSizeAddr (), attributes.GetArrayAddr ());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSearchOperation->SearchExt (NS_ConvertASCIItoUCS2(dn).get(), scope,
            NS_ConvertASCIItoUCS2(filter).get(),
            attributes.GetSize (), attributes.GetArray (),
            mTimeOut, mResultLimit);
    NS_ENSURE_SUCCESS(rv, rv);

    mBound = PR_TRUE;
    return rv;
}

nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageSearchEntry (nsILDAPMessage *aMessage,
        nsIAbDirectoryQueryResult** result)
{
    nsresult rv;
    nsCOMPtr<nsISupportsArray> propertyValues;

    CharPtrArrayGuard properties;
    rv = mQueryArguments->GetReturnProperties (properties.GetSizeAddr(), properties.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    CharPtrArrayGuard attrs;
    rv = aMessage->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString n;
    for (PRUint32 i = 0; i < properties.GetSize(); i++)
    {
        n.Assign (properties[i]);

        nsAbDirectoryQueryPropertyValue* _propertyValue = 0;
        if (n.EqualsWithConversion ("card:URI"))
        {
            // Meta property
            //

            nsXPIDLString dn;
            rv = aMessage->GetDn (getter_Copies (dn));
            NS_ENSURE_SUCCESS(rv, rv);

            nsXPIDLCString uri;
            rv = mDirectoryQuery->CreateCardURI (mUrl, NS_ConvertUCS2toUTF8(dn).get(), getter_Copies (uri));
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ConvertUTF8toUCS2 v (uri.get ());

            _propertyValue = new nsAbDirectoryQueryPropertyValue(n.get (), v.get ());
            if (_propertyValue == NULL)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        else if (n.EqualsWithConversion ("card:nsIAbCard"))
        {
            // Meta property
            //

            nsXPIDLString dn;
            rv = aMessage->GetDn (getter_Copies (dn));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIAbCard> card;
            rv = mDirectoryQuery->CreateCard (mUrl, NS_ConvertUCS2toUTF8(dn).get(), getter_AddRefs (card));
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool hasSetCardProperty = PR_FALSE;
            rv = MozillaLdapPropertyRelator::createCardPropertyFromLDAPMessage (aMessage,
                    card,
                    &hasSetCardProperty);
            NS_ENSURE_SUCCESS(rv, rv);

            if (hasSetCardProperty == PR_FALSE)
                continue;

            _propertyValue = new nsAbDirectoryQueryPropertyValue(n.get (), card);
            if (_propertyValue == NULL)
                return NS_ERROR_OUT_OF_MEMORY;

        }
        else
        {
            const MozillaLdapPropertyRelation* p =
                MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (n.get ());

            if (!p)
                continue;

            for (PRUint32 j = 0; j < attrs.GetSize(); j++)
            {
                if (nsCRT::strcasecmp (p->ldapProperty, attrs[j]) == 0)
                {
                    PRUnicharPtrArrayGuard vals;
                    rv = aMessage->GetValues (attrs[j], vals.GetSizeAddr(), vals.GetArrayAddr());
                    NS_ENSURE_SUCCESS(rv, rv);

                    if (vals.GetSize() == 0)
                        break;

                    _propertyValue = new nsAbDirectoryQueryPropertyValue(n.get (), vals[0]);
                    if (_propertyValue == NULL)
                        return NS_ERROR_OUT_OF_MEMORY;
                    break;
                }
            }
        }

        if (_propertyValue)
        {
            nsCOMPtr<nsIAbDirectoryQueryPropertyValue> propertyValue;
            propertyValue = _propertyValue;

            if (!propertyValues)
            {
                NS_NewISupportsArray(getter_AddRefs(propertyValues));
            }

            propertyValues->AppendElement (propertyValue);
        }
    }

    if (!propertyValues)
        return NS_OK;

    return QueryResultStatus (propertyValues, result,nsIAbDirectoryQueryResult::queryResultMatch);
}
nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageSearchResult (nsILDAPMessage *aMessage,
        nsIAbDirectoryQueryResult** result)
{
    mDirectoryQuery->RemoveListener (mContextID);

    nsresult rv;
    PRInt32 errorCode;
    rv = aMessage->GetErrorCode(&errorCode);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (errorCode == nsILDAPErrors::SUCCESS || errorCode == nsILDAPErrors::SIZELIMIT_EXCEEDED)
        rv = QueryResultStatus (nsnull, result,nsIAbDirectoryQueryResult::queryResultComplete);
    else
        rv = QueryResultStatus (nsnull, result, nsIAbDirectoryQueryResult::queryResultError);

    return rv;
}
nsresult nsAbQueryLDAPMessageListener::QueryResultStatus (nsISupportsArray* properties,
        nsIAbDirectoryQueryResult** result, PRUint32 resultStatus)
{
    nsAbDirectoryQueryResult* _queryResult = new nsAbDirectoryQueryResult (
        mContextID,
        mQueryArguments,
        resultStatus,
        (resultStatus == nsIAbDirectoryQueryResult::queryResultMatch) ? properties : 0);

    if (_queryResult == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = _queryResult;
    NS_IF_ADDREF(*result);

    return NS_OK;
}





NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbLDAPDirectoryQuery, nsIAbDirectoryQuery)

nsAbLDAPDirectoryQuery::nsAbLDAPDirectoryQuery() :
    mInitialized(PR_FALSE),
    mCounter (1),
    mLock (nsnull)
{
    NS_INIT_ISUPPORTS();

}

nsAbLDAPDirectoryQuery::~nsAbLDAPDirectoryQuery()
{
    if(mLock)
        PR_DestroyLock (mLock);
}
nsresult nsAbLDAPDirectoryQuery::Initiate ()
{
    if(mInitialized == PR_TRUE)
        return NS_OK;

    mLock = PR_NewLock ();
    if(!mLock)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    mInitialized = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP nsAbLDAPDirectoryQuery::DoQuery(nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32 resultLimit,
        PRInt32 timeOut,
        PRInt32* _retval)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the scope
    nsCAutoString scope;
    PRBool doSubDirectories;
    rv = arguments->GetQuerySubDirectories (&doSubDirectories);
    NS_ENSURE_SUCCESS(rv, rv);
    if (doSubDirectories == PR_TRUE)
        scope = "sub";
    else
        scope = "one";

    // Get the return attributes
    nsCAutoString returnAttributes;
    rv = getLdapReturnAttributes (arguments, returnAttributes);
    NS_ENSURE_SUCCESS(rv, rv);


    // Get the filter
    nsCOMPtr<nsISupports> supportsExpression;
    rv = arguments->GetExpression (getter_AddRefs (supportsExpression));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbBooleanExpression> expression (do_QueryInterface (supportsExpression, &rv));
    nsCString filter;
    rv = nsAbBoolExprToLDAPFilter::Convert (expression, filter);
    NS_ENSURE_SUCCESS(rv, rv);


    // Set up the search ldap url
    nsCOMPtr<nsILDAPURL> directoryUrl;
    rv = GetLDAPURL (getter_AddRefs (directoryUrl));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsXPIDLCString host;
    rv = directoryUrl->GetHost(getter_Copies (host));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 port;
    rv = directoryUrl->GetPort(&port);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString dn;
    rv = directoryUrl->GetDn(getter_Copies (dn));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString ldapSearchUrlString;
    char* _ldapSearchUrlString = PR_smprintf ("ldap://%s:%d/%s?%s?%s?%s",
            host.get (),
            port,
            dn.get (),
            returnAttributes.get (),
            scope.get (),
            filter.get ());
    if (!_ldapSearchUrlString)
        return NS_ERROR_OUT_OF_MEMORY;
    ldapSearchUrlString = _ldapSearchUrlString;
    PR_smprintf_free (_ldapSearchUrlString);

    nsCOMPtr<nsILDAPURL> url;
    url = do_CreateInstance(NS_LDAPURL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->SetSpec(ldapSearchUrlString.get ());
    NS_ENSURE_SUCCESS(rv, rv);


    // Get the ldap connection
    nsCOMPtr<nsILDAPConnection> ldapConnection;
    rv = GetLDAPConnection (getter_AddRefs (ldapConnection));
    NS_ENSURE_SUCCESS(rv, rv);

    // Initiate contextID for message listener
    PRInt32 contextID;
    // Enter lock
    {
        nsAutoLock lock (mLock);
        contextID = mCounter++;
    }
    // Exit lock


    // Initiate LDAP message listener
    nsCOMPtr<nsILDAPMessageListener> messageListener;
    nsAbQueryLDAPMessageListener* _messageListener =
        new nsAbQueryLDAPMessageListener (
                this,
                contextID,
                url,
                ldapConnection,
                arguments,
                listener,
                resultLimit,
                timeOut);
    if (_messageListener == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    messageListener = _messageListener;
    nsVoidKey key (NS_REINTERPRET_CAST(void *,contextID));

    // Enter lock
    {
        nsAutoLock lock1(mLock);
        mListeners.Put (&key, _messageListener);
    }
    // Exit lock

    *_retval = contextID;

    // Now lets initialize the LDAP connection properly. We'll kick
    // off the bind operation in the callback function, |OnLDAPInit()|.
    rv = ldapConnection->Init(host, port, NS_ConvertASCIItoUCS2(dn).get(),
                              messageListener);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

/* void stopQuery (in long contextID); */
NS_IMETHODIMP nsAbLDAPDirectoryQuery::StopQuery(PRInt32 contextID)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAbQueryLDAPMessageListener* _messageListener;  

    // Enter Lock
    {
        nsAutoLock lock(mLock);

        nsVoidKey key (NS_REINTERPRET_CAST(void *,contextID));

         _messageListener = (nsAbQueryLDAPMessageListener* )mListeners.Remove (&key);

        if (!_messageListener)
            return NS_OK;

    }
    // Exit Lock

    rv = _messageListener->Cancel ();

    return rv;
}


void nsAbLDAPDirectoryQuery::setLdapUrl (const char* aLdapUrl)
{
    mLdapUrl = aLdapUrl;
}

nsresult nsAbLDAPDirectoryQuery::RemoveListener (PRInt32 contextID)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoLock lock(mLock);

    nsVoidKey key (NS_REINTERPRET_CAST(void *,contextID));

    mListeners.Remove (&key);

    return NS_OK;
}



nsresult nsAbLDAPDirectoryQuery::getLdapReturnAttributes (
    nsIAbDirectoryQueryArguments* arguments,
    nsCString& returnAttributes)
{
    nsresult rv;

    CharPtrArrayGuard properties;
    rv = arguments->GetReturnProperties (properties.GetSizeAddr(), properties.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString n;
    for (PRUint32 i = 0; i < properties.GetSize(); i++)
    {
        n.Assign (properties[i]);

        if (n.EqualsWithConversion ("card:URI"))
        {
            // Meta property
            //
            continue;
        }
        else if (n.EqualsWithConversion ("card:nsIAbCard"))
        {
            // Meta property
            // require all attributes
            //
            returnAttributes = "";
            break;
        }

        const MozillaLdapPropertyRelation* p=
            MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (n.get ());
        if (!p)
            continue;

        if (i)
            returnAttributes.Append (",");

        returnAttributes.Append (p->ldapProperty);
    }

    return rv;
}

