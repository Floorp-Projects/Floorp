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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *   Paul Sandoz <paul.sandoz@sun.com>
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
#include "nsAbLDAPProperties.h"
#include "nsILDAPErrors.h"
#include "nsILDAPOperation.h"
#include "nsAbUtils.h"

#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"
#include "nsAutoLock.h"
#include "nsIProxyObjectManager.h"
#include "prprf.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"

class nsAbQueryLDAPMessageListener : public nsILDAPMessageListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPMESSAGELISTENER

    nsAbQueryLDAPMessageListener (
        nsAbLDAPDirectoryQuery* directoryQuery,
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
    nsresult DoSearch();


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
    PRBool mWaitingForPrevQueryToFinish;

    nsCOMPtr<nsILDAPOperation> mSearchOperation;

    PRLock* mLock;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbQueryLDAPMessageListener, nsILDAPMessageListener)

nsAbQueryLDAPMessageListener::nsAbQueryLDAPMessageListener (
        nsAbLDAPDirectoryQuery* directoryQuery,
        nsILDAPURL* url,
        nsILDAPConnection* connection,
        nsIAbDirectoryQueryArguments* queryArguments,
        nsIAbDirectoryQueryResultListener* queryListener,
        PRInt32 resultLimit,
        PRInt32 timeOut) :
    mDirectoryQuery (directoryQuery),
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
    mWaitingForPrevQueryToFinish(PR_FALSE),
    mLock(0)

{
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
    if (mInitialized)
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

    if (mFinished || mCanceled)
        return NS_OK;

    mCanceled = PR_TRUE;
    if (!mFinished)
      mWaitingForPrevQueryToFinish = PR_TRUE;

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

    nsCOMPtr<nsIAbDirectoryQueryResult> queryResult;
    if (!cancelOperation)
    {
        switch (messageType)
        {
        case nsILDAPMessage::RES_BIND:
            rv = OnLDAPMessageBind (aMessage);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        case nsILDAPMessage::RES_SEARCH_ENTRY:
            if (!mFinished && !mWaitingForPrevQueryToFinish)
            {
            rv = OnLDAPMessageSearchEntry (aMessage, getter_AddRefs (queryResult));
            NS_ENSURE_SUCCESS(rv, rv);
            }
            break;
        case nsILDAPMessage::RES_SEARCH_RESULT:
            mWaitingForPrevQueryToFinish = PR_FALSE;
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
        // reset because we might re-use this listener...except don't do this
        // until the search is done, so we'll ignore results from a previous
        // search.
        if (messageType == nsILDAPMessage::RES_SEARCH_RESULT)
          mCanceled = mFinished = PR_FALSE;

    }

    if (queryResult)
        rv = mQueryListener->OnQueryItem (queryResult);

    return rv;
}

NS_IMETHODIMP nsAbQueryLDAPMessageListener::OnLDAPInit(nsILDAPConnection *aConn, nsresult aStatus)
{
    nsresult rv;
    nsXPIDLString passwd;

    // Make sure that the Init() worked properly
    NS_ENSURE_SUCCESS(aStatus, aStatus);

    // If mLogin is set, we're expected to use it to get a password.
    //
    if (!mDirectoryQuery->mLogin.IsEmpty()) {
// XXX hack until nsUTF8AutoString exists
#define nsUTF8AutoString nsCAutoString
        nsUTF8AutoString spec;
        PRBool status;

        // we're going to use the URL spec of the server as the "realm" for 
        // wallet to remember the password by / for.
        // 
        rv = mDirectoryQuery->mDirectoryUrl->GetSpec(spec);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit(): GetSpec"
                     " failed\n");
            return NS_ERROR_FAILURE;
        }

        // get the string bundle service
        //
        nsCOMPtr<nsIStringBundleService> 
            stringBundleSvc(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv)); 
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     " error getting string bundle service");
            return rv;
        }

        // get the LDAP string bundle
        //
        nsCOMPtr<nsIStringBundle> ldapBundle;
        rv = stringBundleSvc->CreateBundle(
            "chrome://mozldap/locale/ldap.properties",
            getter_AddRefs(ldapBundle));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     " error creating string bundle"
                     " chrome://mozldap/locale/ldap.properties");
            return rv;
        } 

        // get the title for the authentication prompt
        //
        nsXPIDLString authPromptTitle;
        rv = ldapBundle->GetStringFromName(
            NS_LITERAL_STRING("authPromptTitle").get(), 
            getter_Copies(authPromptTitle));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     "error getting 'authPromptTitle' string from bundle "
                     "chrome://mozldap/locale/ldap.properties");
            return rv;
        }

        // get the host name for the auth prompt
        //
        nsCAutoString host;
        rv = mUrl->GetAsciiHost(host);
        if (NS_FAILED(rv)) {
            return NS_ERROR_FAILURE;
        }

        // hostTemp is only necessary to work around a code-generation 
        // bug in egcs 1.1.2 (the version of gcc that comes with Red Hat 6.2),
        // which is the default compiler for Mozilla on linux at the moment.
        //
        NS_ConvertASCIItoUCS2 hostTemp(host);
        const PRUnichar *hostArray[1] = { hostTemp.get() };

        // format the hostname into the authprompt text string
        //
        nsXPIDLString authPromptText;
        rv = ldapBundle->FormatStringFromName(
            NS_LITERAL_STRING("authPromptText").get(),
            hostArray, sizeof(hostArray) / sizeof(const PRUnichar *),
            getter_Copies(authPromptText));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     "error getting 'authPromptText' string from bundle "
                     "chrome://mozldap/locale/ldap.properties");
            return rv;
        }


        // get the window watcher service, so we can get an auth prompter
        //
        nsCOMPtr<nsIWindowWatcher> windowWatcherSvc = 
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     " couldn't get window watcher service.");
            return rv;
        }

        // get the addressbook window, as it will be used to parent the auth
        // prompter dialog
        //
        nsCOMPtr<nsIDOMWindow> abDOMWindow;
        rv = windowWatcherSvc->GetWindowByName(
            NS_LITERAL_STRING("addressbookWindow").get(), nsnull,
            getter_AddRefs(abDOMWindow));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     " error getting addressbook Window");
            return rv;
        }

        // get the auth prompter itself
        //
        nsCOMPtr<nsIAuthPrompt> authPrompter;
        rv = windowWatcherSvc->GetNewAuthPrompter(
            abDOMWindow, getter_AddRefs(authPrompter));
        if (NS_FAILED(rv)) {
            NS_ERROR("nsAbQueryLDAPMessageListener::OnLDAPInit():"
                     " error getting auth prompter");
            return rv;
        }

        // get authentication password, prompting the user if necessary
        //
        rv = authPrompter->PromptPassword(
            authPromptTitle.get(), authPromptText.get(),
            NS_ConvertUTF8toUCS2(spec).get(),
            nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY, getter_Copies(passwd),
            &status);
        if (NS_FAILED(rv) || !status) {
            return NS_ERROR_FAILURE;
        }
    }

    // Initiate the LDAP operation
    nsCOMPtr<nsILDAPOperation> ldapOperation =
        do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILDAPMessageListener> proxyListener;
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ,
                     NS_GET_IID(nsILDAPMessageListener),
                     NS_STATIC_CAST(nsILDAPMessageListener *, this),
                     PROXY_SYNC | PROXY_ALWAYS,
                     getter_AddRefs(proxyListener));

    rv = ldapOperation->Init(mConnection, proxyListener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    // Bind
    rv = ldapOperation->SimpleBind(NS_ConvertUCS2toUTF8(passwd));
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}

nsresult nsAbQueryLDAPMessageListener::OnLDAPMessageBind (nsILDAPMessage *aMessage)
{
    if (mBound)
        return NS_OK;

    // see whether the bind actually succeeded
    //
    PRInt32 errCode;
    nsresult rv = aMessage->GetErrorCode(&errCode);
    NS_ENSURE_SUCCESS(rv, rv);

    if (errCode != nsILDAPErrors::SUCCESS) {

        // if the login failed, tell the wallet to forget this password
        //
        if ( errCode == nsILDAPErrors::INAPPROPRIATE_AUTH ||
             errCode == nsILDAPErrors::INVALID_CREDENTIALS ) {

            // make sure the wallet service has been created, and in doing so,
            // pass in a login-failed message to tell it to forget this passwd.
            //
            // apparently getting passwords stored in the wallet
            // doesn't require the service to be running, which is why
            // this might not exist yet.
            //
            rv = NS_CreateServicesFromCategory(
                "passwordmanager", mDirectoryQuery->mDirectoryUrl,
                "login-failed");
            if (NS_FAILED(rv)) {
                NS_ERROR("nsLDAPAutoCompleteSession::ForgetPassword(): error"
                         " creating password manager service");
                // not much to do at this point, though conceivably we could 
                // pop up a dialog telling the user to go manually delete
                // this password in the password manager.
            }
        } 

        // XXX this error should be propagated back to the UI somehow
        return NS_OK;
    }

    mBound = PR_TRUE;
    return DoSearch();
}

 nsresult nsAbQueryLDAPMessageListener::DoSearch()
 {
   nsresult rv;
   mCanceled = mFinished = PR_FALSE;

    mSearchOperation = do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIProxyObjectManager> proxyMgr = 
	    do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
	  NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsILDAPMessageListener> proxyListener;
    rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsILDAPMessageListener),
									this, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs(proxyListener));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSearchOperation->Init (mConnection, proxyListener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString dn;
    rv = mUrl->GetDn (dn);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 scope;
    rv = mUrl->GetScope (&scope);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString filter;
    rv = mUrl->GetFilter (filter);
    NS_ENSURE_SUCCESS(rv, rv);

    CharPtrArrayGuard attributes;
    rv = mUrl->GetAttributes (attributes.GetSizeAddr (), attributes.GetArrayAddr ());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSearchOperation->SearchExt (dn, scope, filter,
            attributes.GetSize (), attributes.GetArray (),
            mTimeOut, mResultLimit);
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

    nsCAutoString propertyName;
    for (PRUint32 i = 0; i < properties.GetSize(); i++)
    {
        propertyName.Assign (properties[i]);

        nsAbDirectoryQueryPropertyValue* _propertyValue = 0;
        if (propertyName.Equals("card:nsIAbCard"))
        {
            // Meta property
            nsCAutoString dn;
            rv = aMessage->GetDn (dn);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIAbCard> card;
            rv = mDirectoryQuery->CreateCard (mUrl, dn.get(), getter_AddRefs (card));
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool hasSetCardProperty = PR_FALSE;
            rv = MozillaLdapPropertyRelator::createCardPropertyFromLDAPMessage (aMessage,
                    card,
                    &hasSetCardProperty);
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasSetCardProperty)
                continue;

            _propertyValue = new nsAbDirectoryQueryPropertyValue(propertyName.get (), card);
            if (_propertyValue == NULL)
                return NS_ERROR_OUT_OF_MEMORY;

        }
        else
        {
            if (!MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (propertyName.get ()))
                continue;

            const MozillaLdapPropertyRelation* relation ;

            for (PRUint32 j = 0; j < attrs.GetSize(); j++)
            {
                relation = MozillaLdapPropertyRelator::findMozillaPropertyFromLdap (attrs[j]);
                if (!relation)
                    continue;
                /*
                 * This change is necessary due to a side effect of #124022. The list of
                 * requested attributes is created in reverse order than how they appear
                 * in nsAbLDAPProperties.cpp. Thus while "surname" and "sn" both map to
                 * LastName, "sn" will be returned by findLdapPropertyFromMozilla() as it
                 * appears first in the hash table but "surname" will be returned by the 
                 * request. 
                 *
                 * Rather than simply reversing the order, an alternative is to compare
                 * the mapped Mozilla attributes where a one to one match exists.
                */

                if (nsCRT::strcasecmp (relation->mozillaProperty, propertyName.get()) == 0)
                {
                    PRUnicharPtrArrayGuard vals;
                    rv = aMessage->GetValues (attrs[j], vals.GetSizeAddr(), vals.GetArrayAddr());
                    NS_ENSURE_SUCCESS(rv, rv);

                    if (vals.GetSize() == 0)
                        break;

                    _propertyValue = new nsAbDirectoryQueryPropertyValue(propertyName.get (), vals[0]);
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

    if (!_queryResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(*result = _queryResult);
    return NS_OK;
}





NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbLDAPDirectoryQuery, nsIAbDirectoryQuery)

nsAbLDAPDirectoryQuery::nsAbLDAPDirectoryQuery() :
    mProtocolVersion (nsILDAPConnection::VERSION3),
    mInitialized(PR_FALSE),
    mLock (nsnull)
{
}

nsAbLDAPDirectoryQuery::~nsAbLDAPDirectoryQuery()
{
    if(mLock)
        PR_DestroyLock (mLock);
}
nsresult nsAbLDAPDirectoryQuery::Initiate ()
{
    if (mInitialized)
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
    PRBool alreadyInitialized = mInitialized;
    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the scope
    nsCAutoString scope;
    PRBool doSubDirectories;
    rv = arguments->GetQuerySubDirectories (&doSubDirectories);
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
    nsCString filter;
    rv = nsAbBoolExprToLDAPFilter::Convert (expression, filter);
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
        filter += NS_LITERAL_CSTRING("(objectclass=inetorgperson)");
    }

    // Set up the search ldap url
    rv = GetLDAPURL (getter_AddRefs (mDirectoryUrl));
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
    NS_ENSURE_SUCCESS(rv,rv);

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
        msgListener->mUrl = url;
        return msgListener->DoSearch();
      }
    }


    // Initiate LDAP message listener
    nsAbQueryLDAPMessageListener* _messageListener =
        new nsAbQueryLDAPMessageListener (
                this,
                url,
                ldapConnection,
                arguments,
                listener,
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
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mListener)
            return NS_OK;

    nsAbQueryLDAPMessageListener *listener = 
      NS_STATIC_CAST(nsAbQueryLDAPMessageListener *, 
      NS_STATIC_CAST(nsILDAPMessageListener *, mListener.get()));
    if (listener)
      rv = listener->Cancel ();
    return rv;
}


nsresult nsAbLDAPDirectoryQuery::getLdapReturnAttributes (
    nsIAbDirectoryQueryArguments* arguments,
    nsCString& returnAttributes)
{
    nsresult rv;

    CharPtrArrayGuard properties;
    rv = arguments->GetReturnProperties (properties.GetSizeAddr(), properties.GetArrayAddr());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString propertyName;
    for (PRUint32 i = 0; i < properties.GetSize(); i++)
    {
        propertyName.Assign (properties[i]);

        if (propertyName.Equals("card:nsIAbCard"))
        {
            // Meta property
            // require all attributes
            //
            rv = MozillaLdapPropertyRelator::GetAllSupportedLDAPAttributes(returnAttributes);
            NS_ASSERTION(NS_SUCCEEDED(rv), "GetAllSupportedLDAPAttributes failed");
            break;
        }

        const MozillaLdapPropertyRelation* tableEntry=
            MozillaLdapPropertyRelator::findLdapPropertyFromMozilla (propertyName.get ());
        if (!tableEntry)
            continue;

        if (i)
            returnAttributes.Append (",");

        returnAttributes.Append (tableEntry->ldapProperty);
    }

    return rv;
}

