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

#include "ldap.h"
#include "nsLDAPOperation.h"
#include "nsLDAPConnection.h"
#include "nsLDAPMessage.h"

// constructor
nsLDAPOperation::nsLDAPOperation(class nsLDAPConnection *c)
{
  this->connection = c;
}

// destructor
nsLDAPOperation::~nsLDAPOperation()
{
}

// wrapper for ldap_simple_bind()
//
bool
nsLDAPOperation::SimpleBind(const char *who, const char *passwd)
{
  this->msgId = ldap_simple_bind(this->connection->connectionHandle, who, 
				 passwd);

  if (this->msgId == -1) {
    return false;
  } else {
    return true;
  }
}

// wrapper for ldap_result
int
nsLDAPOperation::Result(int all, struct timeval *timeout, 
		      class nsLDAPMessage *msg)
{
  return ( ldap_result(this->connection->connectionHandle, this->msgId,
		       all, timeout, &(msg->message)) );
}

// wrappers for ldap_search_ext
//
int
nsLDAPOperation::SearchExt(const char *base, // base DN to search
			   int scope, // LDAP_SCOPE_{BASE,ONELEVEL,SUBTREE}
			   const char* filter, // search filter
			   char **attrs, // attribute types to be returned
			   int attrsOnly, // attrs only, or values too?
			   LDAPControl **serverctrls, 
			   LDAPControl **clientctrls,
			   struct timeval *timeoutp, // how long to wait
			   int sizelimit) // max # of entries to return
{
    return ldap_search_ext(this->connection->connectionHandle, base, scope, 
			   filter, attrs, attrsOnly, serverctrls, 
			   clientctrls, timeoutp, sizelimit, &(this->msgId));
}

int
nsLDAPOperation::SearchExt(const char *base, // base DN to search
			   int scope, // LDAP_SCOPE_{BASE,ONELEVEL,SUBTREE}
			   const char* filter, // search filter
			   struct timeval *timeoutp, // how long to wait
			   int sizelimit) // max # of entries to return
{
    return nsLDAPOperation::SearchExt(base, scope, filter, NULL, 0, NULL,
				    NULL, timeoutp, sizelimit);
}

// wrapper for ldap_url_search
//
int
nsLDAPOperation::URLSearch(const char *URL, // the search URL
			   int attrsonly) // skip attribute names?
{
  this->msgId = ldap_url_search(this->connection->connectionHandle, URL, 
				attrsonly);
  return this->msgId;
}
