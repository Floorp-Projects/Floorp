/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Leif Hedstrom <leif@netscape.com>
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

#include "nsLDAP.h"
#include "ldap.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsILDAPService.h"
#include "nsILDAPMessage.h"
#include "nsILDAPMessageListener.h"
#include "nsCOMPtr.h"
#include "nsILDAPServer.h"
#include "nsILDAPConnection.h"
#include "nsILDAPMessage.h"


// 6a89ae33-7a90-430d-888c-0dede53a951a 
//
#define NS_LDAPSERVICE_CID \
{ \
  0x6a89ae33, 0x7a90, 0x430d, \
  {0x88, 0x8c, 0x0d, 0xed, 0xe5, 0x3a, 0x95, 0x1a} \
}

// This is a little "helper" class, we use to store information
// related to one Service entry (one LDAP server).
//
class nsLDAPServiceEntry
{
  public:
    nsLDAPServiceEntry();
    virtual ~nsLDAPServiceEntry();
    PRBool Init();

    inline PRUint32 GetLeases();
    inline void IncrementLeases();
    inline PRBool DecrementLeases();

    inline PRTime GetTimestamp();
    inline void SetTimestamp();

    inline already_AddRefed<nsILDAPServer> GetServer();
    inline PRBool SetServer(nsILDAPServer *aServer);

    inline already_AddRefed<nsILDAPConnection> GetConnection();
    inline void SetConnection(nsILDAPConnection *aConnection);

    inline already_AddRefed<nsILDAPMessage> GetMessage();
    inline void SetMessage(nsILDAPMessage *aMessage);

    inline already_AddRefed<nsILDAPMessageListener> PopListener();
    inline PRBool PushListener(nsILDAPMessageListener *);

    inline PRBool IsRebinding();
    inline void SetRebinding(PRBool);

    inline PRBool DeleteEntry();

  protected:
    PRUint32 mLeases;         // The number of leases currently granted
    PRTime mTimestamp;        // Last time this server was "used"
    PRBool mDelete;           // This entry is due for deletion
    PRBool mRebinding;        // Keep state if we are rebinding or not

    nsCOMPtr<nsILDAPServer> mServer;
    nsCOMPtr<nsILDAPConnection> mConnection;
    nsCOMPtr<nsILDAPMessage> mMessage;

    // Array holding all the pending callbacks (listeners) for this entry
    nsCOMArray<nsILDAPMessageListener> mListeners;  
};

// This is the interface we're implementing.
//
class nsLDAPService : public nsILDAPService, public nsILDAPMessageListener
{
  public: 
    // interface decls
    //
    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPSERVICE
    NS_DECL_NSILDAPMESSAGELISTENER

    // constructor and destructor
    //
    nsLDAPService();
    virtual ~nsLDAPService();
    
    nsresult Init();

  protected:
    nsresult EstablishConnection(nsLDAPServiceEntry *,
                                 nsILDAPMessageListener *);

    // kinda like strtok_r, but with iterators.  for use by 
    // createFilter
    //
    char *NextToken(nsReadingIterator<char> & aIter,
                    nsReadingIterator<char> & aIterEnd);

    // count how many tokens are in this string; for use by
    // createFilter; note that unlike with NextToken, these params
    // are copies, not references.
    //
    PRUint32 CountTokens(nsReadingIterator<char> aIter,
                         nsReadingIterator<char> aIterEnd);
                   
    
    PRLock *mLock;              // Lock mechanism
    nsHashtable *mServers;      // Hash table holding server entries
    nsHashtable *mConnections;  // Hash table holding "reverse"
                                // lookups from connection to server
};
