/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "nscore.h"
#include "nsIPSMComponent.h"

#define NS_PSMCOMPONENT_CID {0xddcae170, 0x5412, 0x11d3, {0xbb, 0xc8, 0x00, 0x00, 0x86, 0x1d, 0x12, 0x37}}

// Implementation of the PSM app shell component interface.
class nsPSMComponent : public nsIPSMComponent
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_PSMCOMPONENT_CID );

  nsPSMComponent();
  virtual ~nsPSMComponent();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIPSMCOMPONENT

  static NS_METHOD CreatePSMComponent(nsISupports* aOuter, REFNSIID aIID, void **aResult);

private:
  
  PCMT_CONTROL mControl;
  
  nsCOMPtr<nsISupports> mSecureBrowserIU;
  static nsPSMComponent* mInstance;
  nsresult PassAllPrefs();
}; 
