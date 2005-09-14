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

#include "nsLDAPBERElement.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsLDAPBERValue.h"

NS_IMPL_ISUPPORTS1(nsLDAPBERElement, nsILDAPBERElement)

nsLDAPBERElement::nsLDAPBERElement()
  : mElement(0)
{
}

nsLDAPBERElement::~nsLDAPBERElement()
{
  if (mElement) {
    // anything inside here is not something that we own separately from
    // this object, so free it
    ber_free(mElement, 1);
  }

  return;
}

NS_IMETHODIMP
nsLDAPBERElement::Init(nsILDAPBERValue *aValue)
{
  if (aValue) {
    return NS_ERROR_NOT_IMPLEMENTED;
  } 

  mElement = ber_alloc_t(LBER_USE_DER);
  return mElement ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* void putString (in AUTF8String aString, in unsigned long aTag); */
NS_IMETHODIMP
nsLDAPBERElement::PutString(const nsACString & aString, PRUint32 aTag, 
                            PRUint32 *aBytesWritten)
{
  // XXX if the string translation feature of the C SDK is ever used,
  // this const_cast will break
  int i = ber_put_ostring(mElement, 
                          NS_CONST_CAST(char *, 
                                        PromiseFlatCString(aString).get()),
                          aString.Length(), aTag);

  if (i < 0) {
    return NS_ERROR_FAILURE;
  }

  *aBytesWritten = i;
  return NS_OK;
}

/* void startSet (); */
NS_IMETHODIMP nsLDAPBERElement::StartSet(PRUint32 aTag)
{
  int i = ber_start_set(mElement, aTag);

  if (i < 0) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/* void putSet (); */
NS_IMETHODIMP nsLDAPBERElement::PutSet(PRUint32 *aBytesWritten)
{
  int i = ber_put_set(mElement);

  if (i < 0) {
    return NS_ERROR_FAILURE;
  }

  *aBytesWritten = i;
  return NS_OK;
}

/* nsILDAPBERValue flatten (); */
NS_IMETHODIMP nsLDAPBERElement::GetAsValue(nsILDAPBERValue **_retval)
{
  // create the value object
  nsCOMPtr<nsILDAPBERValue> berValue;
  NS_NEWXPCOM(berValue, nsLDAPBERValue);

  if (!berValue) {
    NS_ERROR("nsLDAPBERElement::GetAsValue(): out of memory"
             " creating nsLDAPBERValue object");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  struct berval *bv;
  if ( ber_flatten(mElement, &bv) < 0 ) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = berValue->Set(bv->bv_len, 
                              NS_REINTERPRET_CAST(PRUint8 *, bv->bv_val));

  // whether or not we've succeeded, we're done with the ldap c sdk struct
  ber_bvfree(bv);

  // as of this writing, this error can only be NS_ERROR_OUT_OF_MEMORY
  if (NS_FAILED(rv)) {
    return rv;
  }

  // return the raw interface pointer
  NS_ADDREF(*_retval = berValue.get());

  return NS_OK;
}
