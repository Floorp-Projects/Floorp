/* 
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
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _nsLDAPConnection_h_
#define _nsLDAPConnection_h_

#include "nsILDAPConnection.h"
#include "ldap.h"
#include "nsString.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsCOMPtr.h"
#include "nsILDAPMessageListener.h"
#include "nsHashtable.h"
#include "nspr.h"

// 0d871e30-1dd2-11b2-8ea9-831778c78e93
//
#define NS_LDAPCONNECTION_CID \
{ 0x0d871e30, 0x1dd2, 0x11b2, \
 { 0x8e, 0xa9, 0x83, 0x17, 0x78, 0xc7, 0x8e, 0x93 }}

class nsLDAPConnection : public nsILDAPConnection, nsIRunnable
{
    friend class nsLDAPOperation;
    friend class nsLDAPMessage;

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSILDAPCONNECTION

    // constructor & destructor
    //
    nsLDAPConnection();
    virtual ~nsLDAPConnection();

  protected:

    // invoke the callback associated with a given message, and possibly 
    // delete it from the connection queue
    //
    nsresult InvokeMessageCallback(LDAPMessage *aMsgHandle, 
				   nsILDAPMessage *aMsg,
				   PRInt32 aReturnCode,
				   PRBool aRemoveOpFromConnQ);

    LDAP *mConnectionHandle;		// the LDAP C SDK's connection object
    nsCString *mBindName; 		// who to bind as
    nsCOMPtr<nsIThread> mThread;       	// thread which marshals results

    nsSupportsHashtable *mPendingOperations; // keep these around for callbacks
};

#endif /* _nsLDAPConnection_h_ */
