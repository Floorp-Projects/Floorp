/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsILDAPConnection.h"
#include "nsILDAPOperation.h"
#include "nsILDAPMessageListener.h"
#include "nsILDAPURL.h"
#include "nsString.h"
#include "nsILDAPSyncQuery.h"

// DDDEE14E-ED81-4182-9323-C2AB22FBA68E
#define NS_LDAPSYNCQUERY_CID \
{ 0xdddee14e, 0xed81, 0x4182, \
 { 0x93, 0x23, 0xc2, 0xab, 0x22, 0xfb, 0xa6, 0x8e }}


class nsLDAPSyncQuery : public nsILDAPSyncQuery,
                        public nsILDAPMessageListener

{
  public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPMESSAGELISTENER
    NS_DECL_NSILDAPSYNCQUERY

    nsLDAPSyncQuery();
    virtual ~nsLDAPSyncQuery();

  protected:

    nsCOMPtr<nsILDAPConnection> mConnection; // connection used for search
    nsCOMPtr<nsILDAPOperation> mOperation;   // current ldap op
    nsCOMPtr<nsILDAPURL> mServerURL;         // LDAP URL
    bool mFinished;                        // control variable for eventQ
    PRUint32 mAttrCount;                     // No. of attrbiutes
    char **mAttrs;                           // Attributes to search
    nsString mResults;                       // values to return
    PRUint32 mProtocolVersion;               // LDAP version to use

    nsresult InitConnection();
    // check that we bound ok and start then call StartLDAPSearch
    nsresult OnLDAPBind(nsILDAPMessage *aMessage); 

    // add to the results set
    nsresult OnLDAPSearchEntry(nsILDAPMessage *aMessage); 


    nsresult OnLDAPSearchResult(nsILDAPMessage *aMessage); 

    // kick off a search
    nsresult StartLDAPSearch();
    
    // Clean up after the LDAP Query is done.
    void FinishLDAPQuery();
};

