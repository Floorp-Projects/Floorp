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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
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

#ifndef _nsLDAPMessage_h_
#define _nsLDAPMessage_h_

#include "ldap.h"
#include "nsILDAPMessage.h"
#include "nsILDAPOperation.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"

// 76e061ad-a59f-43b6-b812-ee6e8e69423f
//
#define NS_LDAPMESSAGE_CID \
{ 0x76e061ad, 0xa59f, 0x43b6, \
  { 0xb8, 0x12, 0xee, 0x6e, 0x8e, 0x69, 0x42, 0x3f }}

class nsLDAPMessage : public nsILDAPMessage
{
    friend class nsLDAPOperation;
    friend class nsLDAPConnection;
    friend class nsLDAPConnectionLoop;
    friend PRBool PR_CALLBACK CheckLDAPOperationResult(nsHashKey *aKey, 
                                                       void *aData,
                                                       void* aClosure);

  public:       

    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPMESSAGE

    // constructor & destructor
    //
    nsLDAPMessage();
    virtual ~nsLDAPMessage();

  protected:
    nsresult IterateAttrErrHandler(PRInt32 aLderrno, PRUint32 *aAttrCount, 
                            char** *aAttributes, BerElement *position);
    nsresult IterateAttributes(PRUint32 *aAttrCount, char** *aAttributes, 
                              PRBool getP);
    nsresult Init(nsILDAPConnection *aConnection, 
                  LDAPMessage *aMsgHandle);
    LDAPMessage *mMsgHandle; // the message we're wrapping
    nsCOMPtr<nsILDAPOperation> mOperation;  // operation this msg relates to

    LDAP *mConnectionHandle; // cached connection this op is on

    // since we're caching the connection handle (above), we need to 
    // hold an owning ref to the relevant nsLDAPConnection object as long
    // as we're around
    //
    nsCOMPtr<nsILDAPConnection> mConnection; 

    // the next five member vars are returned by ldap_parse_result()
    //
    int mErrorCode;
    char *mMatchedDn;
    char *mErrorMessage;
    char **mReferrals;
    LDAPControl **mServerControls;
};

#endif // _nsLDAPMessage_h
