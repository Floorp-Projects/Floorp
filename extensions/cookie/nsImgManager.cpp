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

#include "nsImgManager.h"
#include "nsImages.h"

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// nsImgManager Implementation

NS_IMPL_ISUPPORTS1(nsImgManager, nsIImgManager);

nsImgManager::nsImgManager()
{
  NS_INIT_REFCNT();
}

nsImgManager::~nsImgManager(void)
{
}

nsresult nsImgManager::Init()
{
  IMAGE_RegisterPrefCallbacks();
  return NS_OK;
}


NS_IMETHODIMP nsImgManager::Block(const char * imageURL) {
  ::IMAGE_Block(imageURL);
  return NS_OK;
}

NS_IMETHODIMP nsImgManager::CheckForPermission
    (const char * hostname, const char * firstHostname, PRBool *permission) {
  return ::IMAGE_CheckForPermission(hostname, firstHostname, permission);
}
