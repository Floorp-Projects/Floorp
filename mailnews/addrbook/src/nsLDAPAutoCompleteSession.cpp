/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@netscape.com> (Original Author)
 *                 Leif Hedstrom <leif@netscape.com>
 */

// Work around lack of conditional build logic in codewarrior's
// build system.  The MOZ_LDAP_XPCOM preprocessor symbol is only 
// defined on Mac because noone else needs this weirdness; thus 
// the XP_MAC check first.  This conditional encloses the entire
// file, so that in the case where the ldap option is turned off
// in the mac build, a dummy (empty) object will be generated.
//
#if !defined(XP_MAC) || defined(MOZ_LDAP_XPCOM)

#include "nsString.h"
#include "nsLDAPAutoCompleteSession.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsILDAPURL.h"
#include "nsILDAPService.h"
#include "nsXPIDLString.h"
#include "nspr.h"
#include "nsLDAP.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *sLDAPAutoCompleteLogModule = 0;
#endif

NS_IMPL_ISUPPORTS3(nsLDAPAutoCompleteSession, nsIAutoCompleteSession, 
                   nsILDAPMessageListener, nsILDAPAutoCompleteSession)

nsLDAPAutoCompleteSession::nsLDAPAutoCompleteSession() :
    mState(UNBOUND), 
    mFilterTemplate(NS_LITERAL_STRING(
                      "(|(cn=%v1*%v2-*)(mail=%v1*%v2-*)(sn=%v1*%v2-*))")),
    mMaxHits(100), mMinStringLength(2), mSearchAttrs(0), mSearchAttrsSize(0)
{
    NS_INIT_ISUPPORTS();
}

nsLDAPAutoCompleteSession::~nsLDAPAutoCompleteSession()
{
    if (mSearchAttrs) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mSearchAttrsSize, mSearchAttrs);
    }
}

/* void onStartLookup (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStartLookup(const PRUnichar *searchString, 
                                 nsIAutoCompleteResults *previousSearchResult,
                                         nsIAutoCompleteListener *listener)
{
    nsresult rv; // hold return values from XPCOM calls

#ifdef PR_LOGGING
    // initialize logging, if it hasn't been already
    //
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStartLookup entered\n"));
    
    if (!listener) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): NULL listener"
                 "passed in");
        return NS_ERROR_NULL_POINTER;
    } else {
        mListener = listener;   // save it for later callbacks
    }

    // ignore the empty string, strings with @ in them, and strings
    // that are too short
    //
    if (searchString[0] == 0 ||
        nsDependentString(searchString).FindChar(PRUnichar('@'), 0) != 
        kNotFound || 
        mMinStringLength && nsCRT::strlen(searchString) < mMinStringLength)  {

        FinishAutoCompleteLookup(nsIAutoCompleteStatus::ignored);
        return NS_OK;
    } else {
        mSearchString = searchString;        // save it for later use
    }

    // make sure this was called appropriately.
    //
    if (mState == SEARCHING || mState == BINDING) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): called while "
                 "search already in progress; no lookup started.");
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(mFormatter, "nsLDAPAutoCompleteSession::OnStartLookup(): "
                 "formatter attribute has not been set");

    // see if this is a narrow search that we could potentially do locally
    //
    if (previousSearchResult) {

        // get the string representing previous search results
        //
        nsXPIDLString prevSearchStr;

        rv = previousSearchResult->GetSearchString(
            getter_Copies(prevSearchStr));
        if ( NS_FAILED(rv) ) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): couldn't "
                     "get search string from previousSearchResult");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_FAILURE;
        }

        // does the string actually contain anything?
        //
        if ( prevSearchStr.get() && prevSearchStr.get()[0]) {

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSesion::OnStartLookup(): starting "
                    "narrowing search\n"));

            // XXXdmose for performance, we should really do a local, 
            // synchronous search against the existing dataset instead of 
            // just kicking off a new LDAP search here.  When implementing
            // this, need to be sure that only previous results which did not
            // hit the size limit and were successfully completed are used.
            //
            mState = SEARCHING;
            return StartLDAPSearch();
        }
    }

    // init connection if necesary
    //
    switch (mState) {
    case UNBOUND:

        // initialize the connection.  
        //
        rv = InitConnection();
        if (NS_FAILED(rv)) {

            // InitConnection() will have already called 
            // FinishAutoCompleteLookup for us as necessary
            //
            return rv;
        }

        return NS_OK;

    case BOUND:

        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoComplete::OnStartLookup(): subsequent search "
                "starting"));

        // kick off an LDAP search
        mState = SEARCHING;
        return StartLDAPSearch();

    case INITIALIZING:
        // We don't need to do anything here (for now at least), because
        // we can't really abandon the initialization. If we allowed the
        // initialization to be aborted, we could potentially lock the
        // UI thread again, since the DNS service might be stalled.
        //
        return NS_OK;

    case BINDING:
    case SEARCHING:
        // we should never get here
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): unexpected "
                 "value of mStatus");
        return NS_ERROR_UNEXPECTED;
    }
    
    return NS_ERROR_UNEXPECTED;     /*NOTREACHED*/
}

/* void onStopLookup (); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStopLookup()
{
#ifdef PR_LOGGING
    // initialize logging, if it hasn't been already
    //
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStopLookup entered\n"));

    switch (mState) {
    case UNBOUND:
        // nothing to stop
        return NS_OK;

    case BOUND:
        // nothing to stop
        return NS_OK;

    case INITIALIZING:
        // We can't or shouldn't abor the initialization, because then the
        // DNS service can hang again...
        //
        return NS_OK;

    case BINDING:
    case SEARCHING:
        // Abandon the operation, if there is one
        //
        if (mOperation) {
            nsresult rv = mOperation->Abandon();

            if (NS_FAILED(rv)) {
                // since there's nothing interesting that can or should be
                // done if this abandon failed, warn about it and move on
                //
                NS_WARNING("nsLDAPAutoCompleteSession::OnStopLookup(): "
                           "error calling mOperation->Abandon()");
            }

            // force nsCOMPtr to release mOperation
            //
            mOperation = 0;
        }

        // Set the status properly, set to UNBOUND of we were binding, or
        // to BOUND if we were searching.
        //
        mState = (mState == BINDING ? UNBOUND : BOUND);
    }

    mResultsArray = 0;
    mResults = 0;
    mListener = 0;

    return NS_OK;
}

/* void onAutoComplete (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnAutoComplete(const PRUnichar *searchString, 
                                 nsIAutoCompleteResults *previousSearchResult, 
                                          nsIAutoCompleteListener *listener)
{
    // OnStopLookup should have already been called, so there's nothing to
    // free here.  Additionally, as of this writing, noone is hanging around 
    // waiting for mListener->OnAutoComplete() to be called either, and if 
    // they were, it's unclear what we'd return, since we're not guaranteed 
    // to be in any particular state.  My suspicion is that this method 
    // (nsIAutoCompleteSession::OnAutoComplete) should probably be removed
    // from the interface.

    return NS_OK;
}

/**
 * Messages received are passed back via this function.
 *
 * @arg aMessage  The message that was returned, NULL if none was.
 *
 * void OnLDAPMessage (in nsILDAPMessage aMessage)
 */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    PRInt32 messageType;

    // just in case.
    // XXXdmose the semantics of NULL are currently undefined, but are likely
    // to be defined once we insert timeout handling code into the XPCOM SDK
    // At that time we should figure out if this still the right thing to do.
    // 
    if (!aMessage) {
        return NS_OK;
    }

    // figure out what sort of message was returned
    //
    nsresult rv = aMessage->GetType(&messageType);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPMessage(): unexpected "
                 "error in aMessage->GetType()");
       
        // don't call FinishAutoCompleteLookup(), as this could conceivably 
        // be an anomaly, and perhaps the next message will be ok.  if this
        // really was a problem, this search should eventually get
        // reaped by a timeout (once that code gets implemented).
        //
        return NS_ERROR_UNEXPECTED;
    }

    // if this message is not associated with the current operation, 
    // discard it, since it is probably from a previous (aborted) 
    // operation
    //
    PRBool isCurrent;
    rv = IsMessageCurrent(aMessage, &isCurrent);
    if (NS_FAILED(rv)) {
        // IsMessageCurrent will have logged any necessary errors
        return rv;
    }
    if ( ! isCurrent ) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
               ("nsLDAPAutoCompleteSession::OnLDAPMessage(): received message "
                "from operation other than current one; discarded"));
        return NS_OK;
    }

    // XXXdmose - we may want a small state machine either here or
    // or in the nsLDAPConnection object, to make sure that things are 
    // happening in the right order and timing out appropriately.  this will
    // certainly depend on how timeouts are implemented, and how binding
    // gets is dealt with by nsILDAPService.  also need to deal with the case
    // where a bind callback happens after onStopLookup was called.
    // 
    switch (messageType) {

    case nsILDAPMessage::RES_BIND:

        // a bind has completed
        //
        if (mState != BINDING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP bind "
                    "entry returned after OnStopLookup() called; ignoring"));

            // XXXdmose when nsLDAPService integration happens, need to make 
            // sure that the possibility of having an already bound 
            // connection, due to a previously unaborted bind, doesn't cause 
            // any problems.

            return NS_OK;
        }

        return OnLDAPBind(aMessage);

    case nsILDAPMessage::RES_SEARCH_ENTRY:
        
        // ignore this if OnStopLookup was already called
        //
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "entry returned after OnStopLoookup() called; ignoring"));
            return NS_OK;
        }

        // a search entry has been returned
        //
        return OnLDAPSearchEntry(aMessage);

    case nsILDAPMessage::RES_SEARCH_RESULT:

        // ignore this if OnStopLookup was already called
        //
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "result returned after OnStopLookup called; ignoring"));
            return NS_OK;
        }

        // the search is finished; we're all done
        //  
        return OnLDAPSearchResult(aMessage);

    default:
        
        // Given the LDAP operations nsLDAPAutoCompleteSession uses, we should
        // never get here.  If we do get here in a release build, it's
        // probably a bug, but maybe it's the LDAP server doing something
        // weird.  Might as well try and continue anyway.  The session should
        // eventually get reaped by the timeout code, if necessary.
        //
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPMessage(): unexpected "
                 "LDAP message received");
        return NS_OK;
    }
}

// void onLDAPInit (in nsresult aStatus);
//
NS_IMETHODIMP
nsLDAPAutoCompleteSession::OnLDAPInit(nsresult aStatus)
{
    nsresult rv;        // temp for xpcom return values
    nsCOMPtr<nsILDAPMessageListener> selfProxy;

    // Check the status from the initialization of the LDAP connection
    //
    if (NS_FAILED(aStatus)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // create and initialize an LDAP operation (to be used for the bind)
    //  
    mOperation = do_CreateInstance("@mozilla.org/network/ldap-operation;1", 
                                   &rv);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a proxy object so the callback happens on the main thread
    //
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ,
                              NS_GET_IID(nsILDAPMessageListener), 
                              NS_STATIC_CAST(nsILDAPMessageListener *, this), 
                              PROXY_ASYNC | PROXY_ALWAYS, 
                              getter_AddRefs(selfProxy));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPInit(): couldn't "
                 "create proxy to this object for callback");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // our OnLDAPMessage accepts all result callbacks
    //
    rv = mOperation->Init(mConnection, selfProxy);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED; // this should never happen
    }

    // kick off a bind operation 
    // 
    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession:OnLDAPInit(): initiating "
            "SimpleBind\n"));
    rv = mOperation->SimpleBind(NULL); 
    if (NS_FAILED(rv)) {

        switch (rv) {

        case NS_ERROR_LDAP_SERVER_DOWN:
            // XXXdmose try to rebind for this one?  wait for nsILDAPServer to 
            // see...
            //
        case NS_ERROR_LDAP_CONNECT_ERROR:
        case NS_ERROR_LDAP_ENCODING_ERROR:
        case NS_ERROR_OUT_OF_MEMORY:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPInit(): mSimpleBind "
                    "failed, rv = 0x%lx", rv));
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_UNEXPECTED:
        default:
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_UNEXPECTED;
        }
    }

    // Change our state to binding.
    //
    mState = BINDING;

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPBind(nsILDAPMessage *aMessage)
{

    PRInt32 errCode;

    mOperation = 0;  // done with bind op; make nsCOMPtr release it

    // get the status of the bind
    //
    nsresult rv = aMessage->GetErrorCode(&errCode);
    if (NS_FAILED(rv)) {
        
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPBind(): couldn't get "
                 "error code from aMessage");

        // reset to the default state
        //
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }


    // check to be sure the bind succeeded
    //
    if ( errCode != nsILDAPErrors::SUCCESS) {

        // XXXdmose should this be propagated to the UI somehow?
        //
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_WARNING, 
               ("nsLDAPAutoCompleteSession::OnLDAPBind(): error binding to "
                "LDAP server, errCode = %d", errCode));

        // reset to the default state
        //
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // ok, we're starting a search
    //
    mState = SEARCHING;

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPBind(): initial search "
            "starting\n"));

    return StartLDAPSearch();
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchEntry entered\n"));

    // make sure this is only getting called after StartSearch has initialized
    // the result set
    //
    NS_ASSERTION(mResultsArray,
                 "nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "mResultsArrayItems is uninitialized");

    // Errors in this method return an error (which ultimately gets
    // ignored, since this is being called through an async proxy).
    // But the important thing is that we're bailing out here rather
    // than trying to generate a bogus nsIAutoCompleteItem.  Also note
    // that FinishAutoCompleteLookup is _NOT_ being called here, because
    // this error may just have to do with this particular item.

    // generate an autocomplete item from this message by calling the
    // formatter
    //
    nsCOMPtr<nsIAutoCompleteItem> item;
    nsresult rv = mFormatter->Format(aMessage, getter_AddRefs(item));
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
               ("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                "mFormatter->Format() failed"));
        return NS_ERROR_FAILURE;
    }

    rv = mResultsArray->AppendElement(item);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "mItems->AppendElement() failed");
        return NS_ERROR_FAILURE;
    }

    // remember that something has been returned
    //
    mEntriesReturned++;

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    nsresult rv;        // temp for return vals

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchResult entered\n"));

    // figure out if we succeeded or failed, and set the status 
    // and default index appropriately
    //
    AutoCompleteStatus status;

    switch (mEntriesReturned) {

    case 0:
        // we could potentially keep track of non-fatal errors to the 
        // search, and if there has been more than 1, and there are no entries,
        // we could return |failed| instead of |noMatch|.  It's unclear to me
        // that this actually buys us anything though.
        //
        status = nsIAutoCompleteStatus::noMatch;
        break;

    case 1:
        status = nsIAutoCompleteStatus::matchFound;

        // there's only one match, so the default index should point to it
        //
        rv = mResults->SetDefaultItemIndex(0);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                     "mResults->SetDefaultItemIndex(0) failed");
            mState = BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        }

        break;

    default:
        status = nsIAutoCompleteStatus::matchFound;

        // we must have more than one match, so the default index should be
        // unset (-1)
        //
        rv = mResults->SetDefaultItemIndex(-1);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                     "mResults->SetDefaultItemIndex(-1) failed");
            mState = BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        }

        break;
    }

    // This seems to be necessary for things to work, though I'm not sure
    // why that's true.
    //
    rv = mResults->SetSearchString(mSearchString.get());
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): couldn't "
                 "set search string in results object");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);

        return NS_ERROR_FAILURE;
    }

    // XXXdmose just hang out with a bound connection forever?  waiting
    // for nsILDAPService landing to decide on this
    //
    mState = BOUND;

    // call the mListener's OnAutoComplete and clean up
    //
    FinishAutoCompleteLookup(status);

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::StartLDAPSearch()
{
    nsresult rv; // temp for xpcom return values
    nsCOMPtr<nsILDAPMessageListener> selfProxy; // for callback

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::StartLDAPSearch entered\n"));

    // create and initialize an LDAP operation (to be used for the search
    //  
    mOperation = 
        do_CreateInstance("@mozilla.org/network/ldap-operation;1", &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "create @mozilla.org/network/ldap-operation;1");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a proxy object so the callback happens on the main thread
    //
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                              NS_GET_IID(nsILDAPMessageListener), 
			      NS_STATIC_CAST(nsILDAPMessageListener *, this), 
                              PROXY_ASYNC | PROXY_ALWAYS, 
                              getter_AddRefs(selfProxy));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "create proxy to this object for callback");
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // initialize the LDAP operation object
    //
    rv = mOperation->Init(mConnection, selfProxy);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "initialize LDAP operation");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // get the search filter associated with the directory server url; 
    // it will be ANDed with the rest of the search filter that we're using.
    //
    nsXPIDLCString urlFilter;
    rv = mServerURL->GetFilter(getter_Copies(urlFilter));
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // get the LDAP service, since createFilter is called through it.
    //
    nsCOMPtr<nsILDAPService> ldapSvc = do_GetService(
        "@mozilla.org/network/ldap-service;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "get @mozilla.org/network/ldap-service;1");
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // if urlFilter is unset (or set to the default "objectclass=*"), there's
    // no need to AND in an empty search term, so leave prefix and suffix empty
    //
    nsAutoString prefix, suffix;
    if (urlFilter[0] != 0 && nsCRT::strcmp(urlFilter, "(objectclass=*)")) {

        // if urlFilter isn't parenthesized, we need to add in parens so that
        // the filter works as a term to &
        //
        if (urlFilter[0] != '(') {
            prefix = NS_LITERAL_STRING("(&(") + 
                NS_ConvertUTF8toUCS2(urlFilter) +   
                NS_LITERAL_STRING(")");
        } else {
            prefix = NS_LITERAL_STRING("(&") + 
                NS_ConvertUTF8toUCS2(urlFilter);
        }
        
        suffix = PRUnichar(')');
    }

    // generate an LDAP search filter from mFilterTemplate.  If it's unset,
    // use the default.
    //
#define MAX_AUTOCOMPLETE_FILTER_SIZE 1024
    nsAutoString searchFilter;
    rv = ldapSvc->CreateFilter(MAX_AUTOCOMPLETE_FILTER_SIZE, mFilterTemplate,
                               prefix, suffix, NS_LITERAL_STRING(""), 
                               mSearchString, searchFilter);
    if (NS_FAILED(rv)) {
        switch(rv) {

        case NS_ERROR_OUT_OF_MEMORY:
            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;

        case NS_ERROR_NOT_AVAILABLE:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::StartLDAPSearch(): "
                    "createFilter generated filter longer than max filter "
                    "size of %d", MAX_AUTOCOMPLETE_FILTER_SIZE));
            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;

        case NS_ERROR_INVALID_ARG:
        case NS_ERROR_UNEXPECTED:
        default:

            // all this stuff indicates code bugs
            //
            NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): "
                     "createFilter returned unexpected value");

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_UNEXPECTED;
        }

    }

    // create a result set
    //
    mResults = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch() couldn't"
                 " create " NS_AUTOCOMPLETERESULTS_CONTRACTID);
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a pointer to the array in question now, so that we don't have to
    // keep re-fetching it every time an entry callback happens
    //
    rv = mResults->GetItems(getter_AddRefs(mResultsArray));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch() couldn't "
                 "get results array.");
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // nothing returned yet!
    //
    mEntriesReturned = 0;
    
    // get the base dn to search
    //
    nsXPIDLCString dn;
    rv = mServerURL->GetDn(getter_Copies(dn));
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // and the scope
    //
    PRInt32 scope;
    rv = mServerURL->GetScope(&scope);
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // time to kick off the search.
    //
    // XXXdmose what about timeouts? 
    //
    rv = mOperation->SearchExt(NS_ConvertUTF8toUCS2(dn).get(), scope, 
                               searchFilter.get(), mSearchAttrsSize,
                               NS_CONST_CAST(const char **, mSearchAttrs), 0, 
                               mMaxHits);
    if (NS_FAILED(rv)) {
        switch(rv) {

        case NS_ERROR_LDAP_ENCODING_ERROR:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                    "returned NS_ERROR_LDAP_ENCODING_ERROR"));

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_LDAP_SERVER_DOWN:
            // XXXdmose discuss with leif how to handle this in general in the 
            // LDAP XPCOM SDK.  

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                    "returned NS_ERROR_LDAP_SERVER_DOWN"));

            mState=UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_OUT_OF_MEMORY:

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_OUT_OF_MEMORY;

        case NS_ERROR_LDAP_NOT_SUPPORTED:
        case NS_ERROR_NOT_INITIALIZED:        
        case NS_ERROR_INVALID_ARG:
        default:

            // all this stuff indicates code bugs
            //
            NS_ERROR("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                     "returned unexpected value");

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}

// XXXdmose - stopgap routine until nsLDAPService is working
//
nsresult
nsLDAPAutoCompleteSession::InitConnection()
{
    nsresult rv;        // temp for xpcom return values
    nsCOMPtr<nsILDAPMessageListener> selfProxy;
    
    // create an LDAP connection
    //
    mConnection = do_CreateInstance(
        "@mozilla.org/network/ldap-connection;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): could "
                 "not create @mozilla.org/network/ldap-connection;1");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // have we been properly initialized?
    //
    if (!mServerURL) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): mServerURL "
                 "is NULL");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_NOT_INITIALIZED;
    }

    // host to connect to
    //
    nsXPIDLCString host;
    rv = mServerURL->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // on which port
    //
    PRInt32 port;
    rv = mServerURL->GetPort(&port);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }
        
    // get a proxy object so the callback happens on the main thread
    //
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ,
                              NS_GET_IID(nsILDAPMessageListener), 
                              NS_STATIC_CAST(nsILDAPMessageListener *, this), 
                              PROXY_ASYNC | PROXY_ALWAYS, 
                              getter_AddRefs(selfProxy));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): couldn't "
                 "create proxy to this object for callback");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // Initialize the connection. This will cause an asynchronous DNS
    // lookup to occur, and we'll finish the binding of the connection
    // in the OnLDAPInit() listener function.
    //
    rv = mConnection->Init(host, port, 0, selfProxy);
    if NS_FAILED(rv) {
        switch (rv) {

        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_NOT_AVAILABLE:
        case NS_ERROR_FAILURE:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::InitConnection(): mSimpleBind "
                    "failed, rv = 0x%lx", rv));
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;

        case NS_ERROR_ILLEGAL_VALUE:
        default:
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;
        }
    }

    // set our state to initializing
    //
    mState = INITIALIZING;

    return NS_OK;
}

void
nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(AutoCompleteStatus 
                                                    aACStatus)
{
    nsresult rv=NS_OK; // temp for return value 

    // if there's a listener, inform the listener that the search is over
    //
    if (mListener) {

        if (aACStatus == nsIAutoCompleteStatus::matchFound) {
            rv = mListener->OnAutoComplete(mResults, aACStatus);
        } else {
            rv = mListener->OnAutoComplete(0, aACStatus);
        }
    } else {
        // if there's no listener, something's wrong 
        // 
        NS_ERROR("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                 "called with mListener unset!");
    }

    if (NS_FAILED(rv)) {

        // there's nothing we can actually do here other than warn
        //
        NS_WARNING("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                   "error calling mListener->OnAutoComplete()");
    }

    // we're done with various things; cause nsCOMPtr to release them
    //
    mResultsArray = 0;
    mResults = 0;
    mListener = 0;
    mOperation = 0;

    // If we are unbound, drop the connection (if any)
    //
    if (mState == UNBOUND)
        mConnection = 0;
}

// methods for nsILDAPAutoCompleteSession

// attribute wstring searchFilter;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetFilterTemplate(PRUnichar * *aFilterTemplate)
{
    if (!aFilterTemplate) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFilterTemplate = mFilterTemplate.ToNewUnicode();
    if (!*aFilterTemplate) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetFilterTemplate(const PRUnichar * aFilterTemplate)
{
    mFilterTemplate = aFilterTemplate;

    return NS_OK;
}


// attribute long maxHits;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetMaxHits(PRInt32 *aMaxHits)
{
    if (!aMaxHits) {
        return NS_ERROR_NULL_POINTER;
    }

    *aMaxHits = mMaxHits;
    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetMaxHits(PRInt32 aMaxHits)
{
    if ( aMaxHits < -1 || aMaxHits > 65535) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    mMaxHits = aMaxHits;
    return NS_OK;
}

// attribute nsILDAPURL serverURL; 
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetServerURL(nsILDAPURL * *aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }
    
    NS_IF_ADDREF(*aServerURL = mServerURL);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetServerURL(nsILDAPURL * aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }

    mServerURL = aServerURL;

    // the following line will cause the next call to OnStartLookup to 
    // call InitConnection again.  This will reinitialize all the relevant
    // member variables and kick off an LDAP bind.  By virtue of the magic of 
    // nsCOMPtrs, doing this will cause all the nsISupports-based stuff to
    // be Released, which will eventually result in the old connection being
    // destroyed, and the destructor for that calls ldap_unbind()
    //
    mState = UNBOUND;

    return NS_OK;
}

// attribute unsigned long minStringLength
NS_IMETHODIMP
nsLDAPAutoCompleteSession::GetMinStringLength(PRUint32 *aMinStringLength)
{
    if (!aMinStringLength) {
        return NS_ERROR_NULL_POINTER;
    }

    *aMinStringLength = mMinStringLength;
    return NS_OK;
}
NS_IMETHODIMP
nsLDAPAutoCompleteSession::SetMinStringLength(PRUint32 aMinStringLength)
{
    mMinStringLength = aMinStringLength;

    return NS_OK;
}

// check to see if the message returned is related to our current operation
// if there is no current operation, it's not.  :-)
//
nsresult 
nsLDAPAutoCompleteSession::IsMessageCurrent(nsILDAPMessage *aMessage, 
                                            PRBool *aIsCurrent)
{
    // if there's no operation, this message must be stale (ie non-current)
    //
    if ( !mOperation ) {
        *aIsCurrent = PR_FALSE;
        return NS_OK;
    }

    // get the message id from the current operation
    //
    PRInt32 currentId;
    nsresult rv = mOperation->GetMessageID(&currentId);
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling mOperation->GetMessageId()", rv));
        return NS_ERROR_UNEXPECTED;
    }

    // get the message operation from the message
    //
    nsCOMPtr<nsILDAPOperation> msgOp;
    rv = aMessage->GetOperation(getter_AddRefs(msgOp));
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling aMessage->GetOperation()", rv));
        return NS_ERROR_UNEXPECTED;
    }

    // get the message operation id from the message operation
    //
    PRInt32 msgOpId;
    rv = msgOp->GetMessageID(&msgOpId);
    if (NS_FAILED(rv)) {
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoCompleteSession::IsMessageCurrent(): unexpected "
                "error 0x%lx calling msgOp->GetMessageId()", rv));
        return NS_ERROR_UNEXPECTED;
    }
    
    *aIsCurrent = (msgOpId == currentId);

    return NS_OK;
}

// attribute nsILDAPAutoCompFormatter formatter
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetFormatter(nsILDAPAutoCompFormatter* *aFormatter)
{
    if (! aFormatter ) {
        return NS_ERROR_NULL_POINTER;
    }
    
    NS_IF_ADDREF(*aFormatter = mFormatter);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetFormatter(nsILDAPAutoCompFormatter* aFormatter)
{
    if (! aFormatter ) {
        return NS_ERROR_NULL_POINTER;
    }

    NS_ASSERTION(mState == UNBOUND || mState == BOUND, 
                 "nsLDAPAutoCompleteSession::SetFormatter was called when "
                 "mState was set to something other than BOUND or UNBOUND");

    mFormatter = aFormatter;

    // get and cache the attributes that will be used to do lookups
    //
    nsresult rv = mFormatter->GetAttributes(&mSearchAttrsSize, &mSearchAttrs);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::SetFormatter(): "
                 " mFormatter->GetAttributes failed");
        return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

#endif
