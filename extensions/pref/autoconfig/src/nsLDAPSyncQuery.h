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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitesh Shah <mitesh@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

