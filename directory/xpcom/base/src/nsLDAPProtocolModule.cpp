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

#include "nsLDAPURL.h"
#include "nsIGenericFactory.h"
#include "nsLDAPProtocolHandler.h"
#include "nsLDAPChannel.h"
#include "nsLDAPService.h"
#include "nsLDAPConnection.h"
#include "nsLDAPOperation.h"
#include "nsLDAPMessage.h"

// use the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPProtocolHandler);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsLDAPService, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPConnection);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPOperation);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPMessage);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPURL);

// a table of the CIDs implemented by this module (in this case, just one)
//
static nsModuleComponentInfo components[] =
{
    { "LDAP Protocol Handler", NS_LDAPPROTOCOLHANDLER_CID, 
	  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ldap", 
	  nsLDAPProtocolHandlerConstructor },	
    { "LDAP Service", NS_LDAPSERVICE_CID, "mozilla.network.ldapservice", 
	  nsLDAPServiceConstructor },
    { "LDAP Connection", NS_LDAPCONNECTION_CID,
	  "mozilla.network.ldapconnection", nsLDAPConnectionConstructor },
    { "LDAP Operation", NS_LDAPOPERATION_CID,
	  "mozilla.network.ldapoperation", nsLDAPOperationConstructor },
    { "LDAP Message", NS_LDAPMESSAGE_CID,
	  "mozilla.network.ldapmessage", nsLDAPMessageConstructor },
    { "LDAP URL", NS_LDAPURL_CID,
          "mozilla.network.ldapurl", nsLDAPURLConstructor }
};

// implement the NSGetModule() exported function
//
NS_IMPL_NSGETMODULE("nsLDAPProtocolModule", components);

#ifdef DEBUG
PRLogModuleInfo *gLDAPLogModule = 0;
#endif
