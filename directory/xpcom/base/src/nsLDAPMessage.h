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

#ifndef _nsLDAPMessage_h_
#define _nsLDAPMessage_h_

#include "ldap.h"
#include "nsLDAPOperation.h"

class nsLDAPMessage {
  friend class nsLDAPOperation;

 public:

  // constructor
  //
  nsLDAPMessage(class nsLDAPOperation *op);

  // destructor
  //
  ~nsLDAPMessage(void);

  // turn an error condition associated with this message into an LDAP 
  // errcode
  int GetErrorCode(void);

  // turn an error condition associated with this message into a string
  char *GetErrorString(void);

  // wrapper for ldap_msgtype()
  int Type(void);

  // wrapper for ldap_get_dn()
  char *GetDN(void);

  // wrapper for ldap_{first,next}_attribute()
  char *FirstAttribute(void);
  char *NextAttribute(void);

  // wrapper for ldap_get_values
  char **GetValues(const char *attr);

 protected:
  LDAPMessage *message; // the message we're wrapping
  BerElement *position; // position in the associated attr list
  class nsLDAPOperation *operation;  // the connection this msg is related to
};

#endif /* _nsLDAPMessage_h */
