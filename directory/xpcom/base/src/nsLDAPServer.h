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
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Leif Hedstrom <leif@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsLDAP.h"
#include "nsString.h"
#include "nsILDAPServer.h"
#include "nsILDAPURL.h"

// 8bbbaa54-f316-4271-87c3-d52b5b1c1f5b
#define NS_LDAPSERVER_CID \
{ 0x8bbbaa54, 0xf316, 0x4271, \
  { 0x87, 0xc3, 0xd5, 0x2b, 0x5b, 0x1c, 0x1f, 0x5b}}

class nsLDAPServer : public nsILDAPServer
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPSERVER

    // Constructor & destructor
    //
    nsLDAPServer();
    virtual ~nsLDAPServer();

  protected:
    nsString mKey;          // Unique identifier for this server object
    nsCString mUsername;    // Username / UID
    nsCString mPassword;    // Password to bind with
    nsCString mBindDN;      // DN associated with the UID above
    PRUint32 mSizeLimit;    // Limit the LDAP search to this # of entries
    PRUint32 mProtocolVersion;  // What version of LDAP to use?
    // This "links" to a LDAP URL object, which holds further information
    // related to the LDAP server. Like Host, port, base-DN and scope.
    nsCOMPtr<nsILDAPURL> mURL;
};
