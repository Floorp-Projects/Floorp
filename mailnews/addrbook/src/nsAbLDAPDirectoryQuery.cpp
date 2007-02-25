/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *   Paul Sandoz <paul.sandoz@sun.com>
 *   Mark Banner <mark@standard8.demon.co.uk>
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

#include "nsAbLDAPDirectoryQuery.h"
#include "nsAbBoolExprToLDAPFilter.h"
#include "nsILDAPMessage.h"
#include "nsILDAPErrors.h"
#include "nsILDAPOperation.h"
#include "nsIAbLDAPAttributeMap.h"
#include "nsAbUtils.h"

#include "nsXPIDLString.h"
#include "nsAutoLock.h"
#include "nsIProxyObjectManager.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsAbLDAPDirectory.h"
#include "nsAbLDAPListenerBase.h"

// nsAbLDAPListenerBase inherits nsILDAPMessageListener
class nsAbQueryLDAPMessageListener : public nsAbLDAPListenerBase
{
public:
  NS_DECL_ISUPPORTS

  // Note that the directoryUrl is the details of the ldap directory
  // without any search params or return attributes specified. The searchUrl
  // therefore has the search params and return attributes specified.
  nsAbQueryLDAPMessageListener(nsAbLDAPDirectoryQuery* directoryQuery,
                               nsILDAPURL* directoryUrl,
                               nsILDAPURL* searchUrl,
                               nsILDAPConnection* connection,
                               nsIAbDirectoryQueryArguments* queryArguments,
                               nsIAbDirectoryQueryResultListener* queryListener,
                               const nsACString &login,
                               const PRInt32 resultLimit = -1,
                               const PRInt32 timeOut = 0);
  virtual ~nsAbQueryLDAPMessageListener ();

  // nsILDAPMessageListener
  NS_IMETHOD OnLDAPMessage(nsILDAPMessage *aMessage);

protected:
  nsresult OnLDAPMessageSearchEntry (nsILDAPMessage *aMessage,
                                     nsIAbDirectoryQueryResult** result);
  nsresult OnLDAPMessageSearchResult(nsILDAPMessage *aMessage,
                                     nsIAbDirectoryQueryResult** result);

  nsresult QueryResultStatus (nsISupportsArray* properties,
                              nsIAbDirectoryQueryResult** result,
                              PRUint32 resultStatus);

  friend class nsAbLDAPDirectoryQuery;
  nsresult Cancel();
  nsresult DoTask();

  nsCOMPtr<nsILDAPURL> mSearchUrl;
  nsAbLDAPDirectoryQuery* mDirectoryQuery;
  PRInt32 mContextID;
  nsCOMPtr<nsIAbDirectoryQueryArguments> mQueryArguments;
  nsCOMPtr<nsIAbDirectoryQueryResultListener> mQueryListener;
  PRInt32 mResultLimit;

  PRBool mFinished;
  PRBool mCanceled;
  PRBool mWaitingForPrevQueryToFinish;

  nsCOMPtr<nsILDAPOperation> mSearchOperation;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbQueryLDAPMessageListener, nsILDAPMessageListener)

nsAbQueryLDAPMessageListener::nsAbQueryLDAPMessageListener(
        nsAbLDAPDirectoryQuery* directoryQuery,
        nsILDAPURL* directoryUrl,
        nsILDAPURL* searchUrl,
        nsILDAPConnection* connection,
        nsIAbDirectoryQueryArguments* queryArguments,
        nsIAbDirectoryQueryResultListener* queryListener,
        const nsACString &login,
        const PRInt32 resultLimit,
        const PRInt32 timeOut) :
  nsAbLDAPListenerBase(directoryUrl, connection, login, timeOut),
  mSearchUrl(searchUrl),
  mDirectoryQuery(directoryQuery),
  mQueryArguments(queryArguments),
  mQueryListener(queryListener),
  mResultLimit(resultLimit),
  mFinished(PR_FALSE),
  mCanceled(PR_FALSE),
  mWaitingForPrevQueryToFinish(PR_FALSE)
{
}

nsAbQueryLDAPMessageListener::~nsAbQueryLDAPMessageListener ()
{
}

nsresult nsAbQueryLDAPMessageListener::Cancel ()
{
    nsresult rv = Initiate();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoLock lock(mLock);

    if (mFinished || mCanceled)
        return NS_OK;

    mCanceled = PR_TRUE;
    if (!mFinished)
      mWaitingForPrevQueryToFinish = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP nsAbQueryLDAPMessageListener::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    nsresult rv = Initiate();
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 messageType;
    rv = aMessage->GetType(&messageType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool cancelOperation = PR_FALSE;

    // Enter lock
    {
        nsAutoLock lock (mLock);

        if (mFinished)
            return NS_OK;

        if (messageType == nsILDAPMessage::RES_SEARCH_RESULT)
            mFinished = PR_TRUE;
        else if (mCanceled)
        {
            mFinished = PR_TRUE;
            cancelOperation = PR_TRUE;
        }
    }
    // Leave lock

    if (!mDirectoryQuery)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIAbDirectoryQueryResult> queryResult;
    if (!cancelOperation)
    {
        switch (messageType)
        {
        case nsILDAPMessage::RES_BIND:
            rv = OnLDAPMessageBind(aMessage);
            if (NS_FAILED(rv))
              rv = QueryResultStatus(nsnull, getter_AddRefs(queryResult),
                                     nsIAbDirectoryQueryResult::queryResultError);
            break;
        case nsILDAPMessage::RES_SEARCH_ENTRY:
            if (!mFinished && !mWaitingForPrevQueryToFinish)
            {
                rv = OnLDAPMessageSearchEntry(aMessage, 
                                              getter_AddRefs(queryResult));
            }
            break;
        case nsILDAPMessage::RES_SEARCH_RESULT:
            mWaitingForPrevQueryToFinish = PR_FALSE;
            rv = OnLDAPMessageSearchResult(aMessage, getter_AddRefs(queryResult));
            NS_ENSURE_SUCCESS(rv, rv);
        default:
            break;
        }
    }
    else
    {
        if (mSearchOperation)
            rv = mSearchOperation->AbandonExt();

        rv = QueryResultStatus(nsnull, getter_AddRefs(queryResult),
                               nsIAbDirectoryQueryResult::queryResultStopped);
        // reset because we might re-use this listener...except don't do this
        // until the search is done, so we'll ignore results from a previous
        // search.
        if (messageType == nsILDAPMessage::RES_SEARCH_RESULT)
          mCanceled = mFinished = PR_FALSE;

    }

    if (queryResult && mQueryListener)
        rv = mQueryListener->OnQueryItem (queryResult);

    return rv;
}

nsresult nsAbQueryLDAPMessageListener::DoTask()
{
  nsresult rv;
  mCanceled = mFinished = PR_FALSE;

  mSearchOperation = do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILDAPMessageListener> proxyListener;
  rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                            NS_GET_IID(nsILDAPMessageListener),
                            this, NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                            getter_AddRefs(proxyListener));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSearchOperation->Init (mConnection, proxyListener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString dn;
  rv = mSearchUrl->GetDn(dn);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 scope;
  rv = mSearchUrl->GetScope(&scope);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString filter;
  rv = mSearchUrl->GetFilter(filter);
  NS_ENSURE_SUCCESS(rv, rv);

  CharPtrArrayGuard attributes;
  rv = mSearchUrl->GetAttributes(attributes.GetSizeAddr(),
                                 attributes.GetArrayAddr());
  NS_ENSURE_SUCCESS(rv, rv);

  // I don't _think_ it's ever actually possible to get here without having
  // an nsAbLDAPDirectory object, but, just in case, I'll do a QI instead
  // of just static casts...
  nsCOMPtr<nsIAbLDAPDirectory> abLDAPDir =
    do_QueryInterface(mDirectoryQuery, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMutableArray> searchControls;
  rv = abLDAPDir->GetSearchServerControls(getter_AddRefs(searchControls));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSearchOperation->SetServerControls(searchControls);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = abLDAPDir->GetSearchClientControls(getter_AddRefs(searchControls));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSearchOperation->SetClientControls(searchControls);
  NS_ENSURE_SUCCESS(rv, rv);

  return mSearchOperation->SearchExt(dn, scope, filter, attributes.GetSize(),
                                     attributes.GetArray(), mTimeOut,
                                     mResultLimit);
}

nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageSearchEntry (nsILDAPMessage *aMessage,
        nsIAbDirectoryQueryResult** result)
{
    nsresult rv;

    if (!mDirectoryQuery)
      return NS_ERROR_NULL_POINTER;

    // the address book fields that we'll be asking for
    CharPtrArrayGuard properties;
    rv = mQueryArguments->GetReturnProperties(properties.GetSizeAddr(),
                                              properties.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    // the map for translating between LDAP attrs <-> addrbook fields
    nsCOMPtr<nsISupports> iSupportsMap;
    rv = mQueryArguments->GetTypeSpecificArg(getter_AddRefs(iSupportsMap));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAbLDAPAttributeMap> map = do_QueryInterface(iSupportsMap, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // set up variables for handling the property values to be returned
    nsCOMPtr<nsISupportsArray> propertyValues;
    nsCOMPtr<nsIAbDirectoryQueryPropertyValue> propertyValue;
    rv = NS_NewISupportsArray(getter_AddRefs(propertyValues));
    NS_ENSURE_SUCCESS(rv, rv);
        
    if (!strcmp(properties[0], "card:nsIAbCard")) {
        // Meta property
        nsCAutoString dn;
        rv = aMessage->GetDn (dn);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAbCard> card;
        rv = mDirectoryQuery->CreateCard(mSearchUrl, dn.get(),
                                         getter_AddRefs(card));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = map->SetCardPropertiesFromLDAPMessage(aMessage, card);
        NS_ENSURE_SUCCESS(rv, rv);

        propertyValue = new nsAbDirectoryQueryPropertyValue(properties[0], card);
        if (!propertyValue) 
            return NS_ERROR_OUT_OF_MEMORY;

        rv = propertyValues->AppendElement(propertyValue);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {

        for (PRUint32 i = 0; i < properties.GetSize(); i++)
            {
                // this is the precedence order list of attrs for this property
                CharPtrArrayGuard attrs;
                rv = map->GetAttributes(nsDependentCString(properties[i]),
                                        attrs.GetSizeAddr(),
                                        attrs.GetArrayAddr());

                // if there are no attrs for this property, just move on
                if (NS_FAILED(rv) || !strlen(attrs[0])) {
                    continue;
                }

                // iterate through list, until first property found
                for (PRUint32 j=0; j < attrs.GetSize(); j++) {

                    // try and get the values for this ldap attribute
                    PRUnicharPtrArrayGuard vals;
                    rv = aMessage->GetValues(attrs[j], vals.GetSizeAddr(),
                                             vals.GetArrayAddr());

                    if (NS_SUCCEEDED(rv) && vals.GetSize()) {
                        propertyValue = new nsAbDirectoryQueryPropertyValue(
                            properties[i], vals[0]);
                        if (!propertyValue) {
                            return NS_ERROR_OUT_OF_MEMORY;
                        }

                        (void)propertyValues->AppendElement (propertyValue);
                        break;
                    }
                }
            }
    }

    return QueryResultStatus (propertyValues, result, nsIAbDirectoryQueryResult::queryResultMatch);
}
nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageSearchResult (nsILDAPMessage *aMessage,
        nsIAbDirectoryQueryResult** result)
{
    PRInt32 errorCode;
    nsresult rv = aMessage->GetErrorCode(&errorCode);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (errorCode == nsILDAPErrors::SUCCESS || errorCode == nsILDAPErrors::SIZELIMIT_EXCEEDED)
        rv = QueryResultStatus (nsnull, result,nsIAbDirectoryQueryResult::queryResultComplete);
    else
        rv = QueryResultStatus (nsnull, result, nsIAbDirectoryQueryResult::queryResultError);

    return rv;
}

nsresult nsAbQueryLDAPMessageListener::QueryResultStatus(nsISupportsArray* properties,
        nsIAbDirectoryQueryResult** result, PRUint32 resultStatus)
{
    nsAbDirectoryQueryResult* _queryResult = new nsAbDirectoryQueryResult (
        mContextID,
        mQueryArguments,
        resultStatus,
        (resultStatus == nsIAbDirectoryQueryResult::queryResultMatch) ? properties : 0);

    if (!_queryResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(*result = _queryResult);
    return NS_OK;
}



NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbLDAPDirectoryQuery, nsIAbDirectoryQuery)

nsAbLDAPDirectoryQuery::nsAbLDAPDirectoryQuery() :
    mProtocolVersion (nsILDAPConnection::VERSION3),
    mInitialized(PR_FALSE)
{
}

nsAbLDAPDirectoryQuery::~nsAbLDAPDirectoryQuery()
{
     nsAbQueryLDAPMessageListener *msgListener = 
        NS_STATIC_CAST(nsAbQueryLDAPMessageListener *, 
        NS_STATIC_CAST(nsILDAPMessageListener *, mListener.get()));
     if (msgListener)
     {
       msgListener->mDirectoryQuery = nsnull;
       msgListener->mQueryListener = nsnull;
     }
}

NS_IMETHODIMP nsAbLDAPDirectoryQuery::DoQuery(nsIAbDirectoryQueryArguments* arguments,
        nsIAbDirectoryQueryResultListener* listener,
        PRInt32 resultLimit,
        PRInt32 timeOut,
        PRInt32* _retval)
{
    PRBool alreadyInitialized = mInitialized;
    mInitialized = PR_TRUE;

    // Get the scope
    nsCAutoString scope;
    PRBool doSubDirectories;
    nsresult rv = arguments->GetQuerySubDirectories (&doSubDirectories);
    NS_ENSURE_SUCCESS(rv, rv);
    scope = (doSubDirectories) ? "sub" : "one";

    // Get the return attributes
    nsCAutoString returnAttributes;
    rv = getLdapReturnAttributes (arguments, returnAttributes);
    NS_ENSURE_SUCCESS(rv, rv);


    // Get the filter
    nsCOMPtr<nsISupports> supportsExpression;
    rv = arguments->GetExpression (getter_AddRefs (supportsExpression));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbBooleanExpression> expression (do_QueryInterface (supportsExpression, &rv));
    nsCAutoString filter;

    // figure out how we map attribute names to addressbook fields for this
    // query
    nsCOMPtr<nsISupports> iSupportsMap;
    rv = arguments->GetTypeSpecificArg(getter_AddRefs(iSupportsMap));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbLDAPAttributeMap> map = do_QueryInterface(iSupportsMap, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsAbBoolExprToLDAPFilter::Convert (map, expression, filter);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * Mozilla itself cannot arrive here with a blank filter
     * as the nsAbLDAPDirectory::StartSearch() disallows it.
     * But 3rd party LDAP query integration with Mozilla begins 
     * in this method.
     *
     * Default the filter string if blank, otherwise it gets
     * set to (objectclass=*) which returns everything. Set 
     * the default to (objectclass=inetorgperson) as this 
     * is the most appropriate default objectclass which is 
     * central to the makeup of the mozilla ldap address book 
     * entries.
    */
    if(filter.IsEmpty())
    {
        filter.AssignLiteral("(objectclass=inetorgperson)");
    }

    // Set up the search ldap url
    rv = GetLDAPURL(getter_AddRefs(mDirectoryUrl));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCAutoString host;
    rv = mDirectoryUrl->GetAsciiHost(host);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 port;
    rv = mDirectoryUrl->GetPort(&port);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString dn;
    rv = mDirectoryUrl->GetDn(dn);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 options;
    rv = mDirectoryUrl->GetOptions(&options);
    NS_ENSURE_SUCCESS(rv, rv);

    // get the directoryFilter from the directory url and merge it with the user's
    // search filter
    nsCAutoString urlFilter;
    rv = mDirectoryUrl->GetFilter(urlFilter);

    // if urlFilter is unset (or set to the default "objectclass=*"), there's
    // no need to AND in an empty search term, so leave prefix and suffix empty

    nsCAutoString searchFilter;
    if (urlFilter.Length() && !urlFilter.Equals(NS_LITERAL_CSTRING("(objectclass=*)"))) 
    {

      // if urlFilter isn't parenthesized, we need to add in parens so that
      // the filter works as a term to &
      //
      if (urlFilter[0] != '(') 
        searchFilter = NS_LITERAL_CSTRING("(&(") + urlFilter + NS_LITERAL_CSTRING(")");
      else
        searchFilter = NS_LITERAL_CSTRING("(&") + urlFilter;

      searchFilter += filter;
      searchFilter += ')';
    } 
    else
      searchFilter = filter;

    nsCString ldapSearchUrlString;
    char* _ldapSearchUrlString = 
        PR_smprintf ("ldap%s://%s:%d/%s?%s?%s?%s",
                     (options & nsILDAPURL::OPT_SECURE) ? "s" : "",
                     host.get (),
                     port,
                     dn.get (),
                     returnAttributes.get (),
                     scope.get (),
                     searchFilter.get ());
    if (!_ldapSearchUrlString)
        return NS_ERROR_OUT_OF_MEMORY;
    ldapSearchUrlString = _ldapSearchUrlString;
    PR_smprintf_free (_ldapSearchUrlString);

    nsCOMPtr<nsILDAPURL> url;
    url = do_CreateInstance(NS_LDAPURL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->SetSpec(ldapSearchUrlString);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the ldap connection
    nsCOMPtr<nsILDAPConnection> ldapConnection;
    rv = GetLDAPConnection (getter_AddRefs (ldapConnection));
    NS_ENSURE_SUCCESS(rv, rv);

    // too soon? Do we need a new listener?
    if (alreadyInitialized)
    {
      nsAbQueryLDAPMessageListener *msgListener = 
        NS_STATIC_CAST(nsAbQueryLDAPMessageListener *, 
        NS_STATIC_CAST(nsILDAPMessageListener *, mListener.get()));
      if (msgListener)
      {
        msgListener->mDirectoryUrl = mDirectoryUrl;
        msgListener->mSearchUrl = url;
        return msgListener->DoTask();
      }
    }

    // Initiate LDAP message listener
    nsAbQueryLDAPMessageListener* _messageListener =
        new nsAbQueryLDAPMessageListener (
                this,
                mDirectoryUrl,
                url,
                ldapConnection,
                arguments,
                listener,
                mLogin,
                resultLimit,
                timeOut);
    if (_messageListener == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    mListener = _messageListener;
    *_retval = 1;

    // Now lets initialize the LDAP connection properly. We'll kick
    // off the bind operation in the callback function, |OnLDAPInit()|.
    rv = ldapConnection->Init(host.get(), port, options, mLogin,
                              mListener, nsnull, mProtocolVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

/* void stopQuery (in long contextID); */
NS_IMETHODIMP nsAbLDAPDirectoryQuery::StopQuery(PRInt32 contextID)
{
  mInitialized = PR_TRUE;

  if (!mListener)
    return NS_OK;

  nsAbQueryLDAPMessageListener *listener = 
    NS_STATIC_CAST(nsAbQueryLDAPMessageListener *, 
                   NS_STATIC_CAST(nsILDAPMessageListener *, mListener.get()));
  if (listener)
    return listener->Cancel();

  return NS_OK;
}


nsresult nsAbLDAPDirectoryQuery::getLdapReturnAttributes (
    nsIAbDirectoryQueryArguments* arguments,
    nsCString& returnAttributes)
{
    CharPtrArrayGuard properties;
    nsresult rv = arguments->GetReturnProperties(properties.GetSizeAddr(),
                                                 properties.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    // figure out how we map attribute names to addressbook fields for this
    // query
    nsCOMPtr<nsISupports> iSupportsMap;
    rv = arguments->GetTypeSpecificArg(getter_AddRefs(iSupportsMap));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbLDAPAttributeMap> map = do_QueryInterface(iSupportsMap, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!strcmp(properties[0], "card:nsIAbCard")) {
        // Meta property
        // require all attributes
        //
        rv = map->GetAllCardAttributes(returnAttributes);
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetAllSupportedAttributes failed");
        return rv;
    }

    PRBool needComma = PR_FALSE;
    for (PRUint32 i = 0; i < properties.GetSize(); i++)
    {
        nsCAutoString attrs;

        // get all the attributes for this property
        rv = map->GetAttributeList(nsDependentCString(properties[i]), attrs);

        // if there weren't any attrs, just keep going
        if (NS_FAILED(rv) || attrs.IsEmpty()) {
            continue;
        }

        // add a comma, if necessary
        if (needComma) {
            returnAttributes.Append(PRUnichar (','));
        }

        returnAttributes.Append(attrs);

        // since we've added attrs, we definitely need a comma next time
        // we're here
        needComma = PR_TRUE;
    }

    return rv;
}
