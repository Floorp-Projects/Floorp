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

#include "nsLDAPMessage.h"
#include <stdio.h>

// constructor
nsLDAPMessage::nsLDAPMessage(class nsLDAPOperation *op)
{
  this->operation = op;
  this->message = NULL;
  this->position = NULL;
}

// destructor
// XXX better error-handling than fprintf
//
nsLDAPMessage::~nsLDAPMessage(void)
{
  int rc;

  if (this->message != NULL) {
    rc = ldap_msgfree(this->message);

    switch(rc) {
    case LDAP_RES_BIND:
    case LDAP_RES_SEARCH_ENTRY:
    case LDAP_RES_SEARCH_RESULT:
    case LDAP_RES_MODIFY:
    case LDAP_RES_ADD:
    case LDAP_RES_DELETE:
    case LDAP_RES_MODRDN:
    case LDAP_RES_COMPARE:
    case LDAP_RES_SEARCH_REFERENCE:
    case LDAP_RES_EXTENDED:
    case LDAP_RES_ANY:
      // success
      break;
    case LDAP_SUCCESS:
      // timed out (dunno why LDAP_SUCESS is used to indicate this) 
      fprintf(stderr, 
	      "nsLDAPMessage::~nsLDAPMessage: ldap_msgfree() timed out.\n");
      fflush(stderr);
      break;
    default:
      // other failure
      // XXX - might errno conceivably be useful here?
      fprintf(stderr,"nsLDAPMessage::~nsLDAPMessage: ldap_msgfree: %s\n",
	      ldap_err2string(rc));
      fflush(stderr);
      break;
    }
  }

  if ( this->position != NULL ) {
      ldap_ber_free(this->position, 0);
  }

}

// XXX - both this and GetErrorString should be based on a separately
// broken out ldap_parse_result
//
int
nsLDAPMessage::GetErrorCode(void)
{
  int errcode;
  int rc;

  rc = ldap_parse_result(this->operation->connection->connectionHandle,
			 this->message, &errcode, NULL, NULL,
			 NULL, NULL, 0);
  if (rc != LDAP_SUCCESS) {
    fprintf(stderr, "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
	    ldap_err2string(rc));
    exit(-1);
  }

  return(errcode);
}

// XXX deal with extra params (make client not have to use ldap_memfree() on 
// result)
// XXX better error-handling than fprintf()
//
char *
nsLDAPMessage::GetErrorString(void)
{
  int errcode;
  char *matcheddn;
  char *errmsg;
  char **referrals;
  LDAPControl **serverctrls;

  int rc;

  rc = ldap_parse_result(this->operation->connection->connectionHandle,
			 this->message, &errcode, &matcheddn, &errmsg,
			 &referrals, &serverctrls, 0);
  if (rc != LDAP_SUCCESS) {
    fprintf(stderr, "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
	    ldap_err2string(rc));
    exit(-1);
  }

  if (matcheddn) ldap_memfree(matcheddn);
  if (errmsg) ldap_memfree(errmsg);
  if (referrals) ldap_memfree(referrals);
  if (serverctrls) ldap_memfree(serverctrls);

  return(ldap_err2string(errcode));
}

// wrapper for ldap_first_attribute 
//
char *
nsLDAPMessage::FirstAttribute(void)
{
    return ldap_first_attribute(this->operation->connection->connectionHandle,
				this->message, &(this->position));
}

// wrapper for ldap_next_attribute()
//
char * 
nsLDAPMessage::NextAttribute(void)
{
    return ldap_next_attribute(this->operation->connection->connectionHandle,
			       this->message, this->position);
}

// wrapper for ldap_msgtype()
//
int
nsLDAPMessage::Type(void)
{
    return (ldap_msgtype(this->message));
}

// wrapper for ldap_get_dn
//
char * 
nsLDAPMessage::GetDN(void)
{
    return ldap_get_dn(this->operation->connection->connectionHandle,
		       this->message);
}

// wrapper for ldap_get_values()
//
char **
nsLDAPMessage::GetValues(const char *attr)
{
    return ldap_get_values(this->operation->connection->connectionHandle,
			   this->message, attr);
}
