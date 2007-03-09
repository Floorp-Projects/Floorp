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
 *   Dan Mosedale <dmose@mozilla.org>
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

#ifndef _nsLDAPOperation_h_
#define _nsLDAPOperation_h_

#include "ldap.h"
#include "nsCOMPtr.h"
#include "nsILDAPConnection.h"
#include "nsILDAPOperation.h"
#include "nsILDAPMessageListener.h"
#include "nsString.h"
#include "nsIMutableArray.h"

// 97a479d0-9a44-47c6-a17a-87f9b00294bb
#define NS_LDAPOPERATION_CID \
{ 0x97a479d0, 0x9a44, 0x47c6, \
  { 0xa1, 0x7a, 0x87, 0xf9, 0xb0, 0x02, 0x94, 0xbb}}

class nsLDAPOperation : public nsILDAPOperation
{
  public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPOPERATION

    // constructor & destructor
    //
    nsLDAPOperation();
    virtual ~nsLDAPOperation();

    /**
     * used to break cycles
     */
    void Clear();

  private:
    /**
     * wrapper for ldap_add_ext()
     *
     * XXX should move to idl, once LDAPControls have an IDL representation
     */
    nsresult AddExt(const char *base, // base DN to add
                    nsIArray *mods, // Array of modifications
                    LDAPControl **serverctrls,
                    LDAPControl **clientctrls);

    /**
     * wrapper for ldap_delete_ext()
     *
     * XXX should move to idl, once LDAPControls have an IDL representation
     */
    nsresult DeleteExt(const char *base, // base DN to delete
                       LDAPControl **serverctrls,
                       LDAPControl **clientctrls);

    /**
     * wrapper for ldap_modify_ext()
     *
     * XXX should move to idl, once LDAPControls have an IDL representation
     */
    nsresult ModifyExt(const char *base, // base DN to modify
                       nsIArray *mods, // array of modifications
                       LDAPControl **serverctrls,
                       LDAPControl **clientctrls);

    /**
     * wrapper for ldap_rename()
     *
     * XXX should move to idl, once LDAPControls have an IDL representation
     */
    nsresult Rename(const char *base, // base DN to rename
                    const char *newRDn, // new RDN
                    const char *newParent, // DN of the new parent
                    PRBool deleteOldRDn, // remove old RDN in the entry?
                    LDAPControl **serverctrls,
                    LDAPControl **clientctrls);

    /**
     * Helper function to copy the values of an nsILDAPModification into an
     * array of berval's.
     */
    static nsresult CopyValues(nsILDAPModification* aMod, berval*** aBValues);

    nsCOMPtr<nsILDAPMessageListener> mMessageListener; // results go here
    nsCOMPtr<nsISupports> mClosure;  // private parameter (anything caller desires)
    nsCOMPtr<nsILDAPConnection> mConnection; // connection this op is on

    LDAP *mConnectionHandle; // cache connection handle
    nsCString mSavePassword;
    PRInt32 mMsgID;          // opaque handle to outbound message for this op

    nsCOMPtr<nsIMutableArray> mClientControls;
    nsCOMPtr<nsIMutableArray> mServerControls;
};

#endif // _nsLDAPOperation_h
