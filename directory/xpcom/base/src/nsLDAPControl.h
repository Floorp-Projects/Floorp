/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dan.mosedale@oracle.com> (original author)
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

#include "nsILDAPControl.h"
#include "nsCOMPtr.h"
#include "nsILDAPBERValue.h"
#include "nsString.h"
#include "ldap.h"

// {5B608BBE-C0EA-4f74-B209-9CDCD79EC401}
#define NS_LDAPCONTROL_CID \
  { 0x5b608bbe, 0xc0ea, 0x4f74, \
      { 0xb2, 0x9, 0x9c, 0xdc, 0xd7, 0x9e, 0xc4, 0x1 } }

class nsLDAPControl : public nsILDAPControl
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILDAPCONTROL

  nsLDAPControl();

  /**
   * return a pointer to C-SDK compatible LDAPControl structure.  Note that
   * this is allocated with NS_Alloc and must be freed with NS_Free, both by 
   * ldap_control_free() and friends.
   *
   * @exception null pointer return if allocation failed
   */
  nsresult ToLDAPControl(LDAPControl **aControl);

private:
  ~nsLDAPControl();

protected:
  nsCOMPtr<nsILDAPBERValue> mValue;	// the value portion of this control
  PRBool mIsCritical;      // should server abort if control not understood?
  nsCString mOid;          // Object ID for this control
};
