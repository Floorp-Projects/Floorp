/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsPermission.h"
#include "nsString.h"
#include "prmem.h"

// nsPermission Implementation

NS_IMPL_ISUPPORTS2(nsPermission, nsIPermission, nsISupportsWeakReference);

nsPermission::nsPermission() {
  NS_INIT_REFCNT();
}

nsPermission::nsPermission
  (char * host,
   PRInt32 type,
   PRBool capability) {
  permissionHost = host;
  permissionType = type;
  permissionCapability = capability;
  NS_INIT_REFCNT();
}

nsPermission::~nsPermission(void) {
  nsCRT::free(permissionHost);
}

NS_IMETHODIMP nsPermission::GetHost(char * *aHost) {
  if (permissionHost) {
    *aHost = (char *) nsMemory::Clone(permissionHost, nsCRT::strlen(permissionHost) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsPermission::GetType(PRInt32 *aType) {
  *aType = permissionType;
  return NS_OK;
}

NS_IMETHODIMP nsPermission::GetCapability(PRBool *aCapability) {
  *aCapability = permissionCapability;
  return NS_OK;
}
