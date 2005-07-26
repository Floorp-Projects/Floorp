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

#include "nsLDAPControl.h"
#include "prmem.h"
#include "plstr.h"
#include "nsLDAPBERValue.h"

NS_IMPL_ISUPPORTS1(nsLDAPControl, nsILDAPControl)

nsLDAPControl::nsLDAPControl()
  : mIsCritical(PR_FALSE)
{
}

nsLDAPControl::~nsLDAPControl()
{
}

/* attribute ACString oid; */
NS_IMETHODIMP nsLDAPControl::GetOid(nsACString & aOid)
{
  aOid.Assign(mOid);
  return NS_OK;
}
NS_IMETHODIMP nsLDAPControl::SetOid(const nsACString & aOid)
{
  mOid = aOid;
  return NS_OK;
}

/* attribute nsILDAPBERValue value; */
NS_IMETHODIMP
nsLDAPControl::GetValue(nsILDAPBERValue * *aValue)
{
  NS_IF_ADDREF(*aValue = mValue);
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPControl::SetValue(nsILDAPBERValue * aValue)
{
  mValue = aValue;
  return NS_OK;
}

/* attribute boolean isCritical; */
NS_IMETHODIMP 
nsLDAPControl::GetIsCritical(PRBool *aIsCritical)
{
  *aIsCritical = mIsCritical;
  return NS_OK;
}
NS_IMETHODIMP
nsLDAPControl::SetIsCritical(PRBool aIsCritical)
{
  mIsCritical = aIsCritical;
  return NS_OK;
}

/**
 * utility routine for use inside the LDAP XPCOM SDK
 */
nsresult
nsLDAPControl::ToLDAPControl(LDAPControl **control)
{
  // because nsLDAPProtocolModule::Init calls prldap_install_routines we know
  // that the C SDK will be using the NSPR allocator under the hood, so our
  // callers will therefore be able to use ldap_control_free() and friends on
  // this control.
  LDAPControl *ctl = NS_STATIC_CAST(LDAPControl *, 
                                    PR_Calloc(1, sizeof(LDAPControl)));
  if (!ctl) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // need to ensure that this string is also alloced by PR_Alloc
  ctl->ldctl_oid = PL_strdup(mOid.get());
  if (!ctl->ldctl_oid) {
    PR_Free(ctl);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ctl->ldctl_iscritical = mIsCritical;

  if (!mValue) {
    // no data associated with this control
    ctl->ldctl_value.bv_len = 0;
    ctl->ldctl_value.bv_val = 0;
  } else {

    // just to make the code below a bit more readable
    nsLDAPBERValue *nsBerVal = 
      NS_STATIC_CAST(nsLDAPBERValue *, NS_STATIC_CAST(nsILDAPBERValue *,
                                                      mValue.get()));
    ctl->ldctl_value.bv_len = nsBerVal->mSize;

    if (!nsBerVal->mSize) {
      // a zero-length value is associated with this control
      return NS_ERROR_NOT_IMPLEMENTED;
    } else {

      // same for the berval itself
      ctl->ldctl_value.bv_len = nsBerVal->mSize;
      ctl->ldctl_value.bv_val = NS_STATIC_CAST(char *,
                                               PR_Malloc(nsBerVal->mSize));
      if (!ctl->ldctl_value.bv_val) {
        ldap_control_free(ctl);
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      memcpy(ctl->ldctl_value.bv_val, nsBerVal->mValue,
             ctl->ldctl_value.bv_len);
    }
  }

  *control = ctl;

  return NS_OK;
}
