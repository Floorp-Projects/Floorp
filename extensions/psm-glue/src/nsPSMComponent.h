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
#ifndef _NSPSMCOMPONENT_H
#define  _NSPSMCOMPONENT_H
#include "nscore.h"
#include "nsIPSMComponent.h"
#include "nsISignatureVerifier.h"
#include "nsIStringBundle.h"

#include "nsIContentHandler.h"
#include "nsIURIContentListener.h"

#define SECURITY_STRING_BUNDLE_URL "chrome://communicator/locale/security.properties"

#define NS_PSMCOMPONENT_CID {0xddcae170, 0x5412, 0x11d3, {0xbb, 0xc8, 0x00, 0x00, 0x86, 0x1d, 0x12, 0x37}}

#define NS_CERTCONTENTLISTEN_CID {0xc94f4a30, 0x64d7, 0x11d4, {0x99, 0x60, 0x00, 0xb0, 0xd0, 0x23, 0x54, 0xa0}}
#define NS_CERTCONTENTLISTEN_CONTRACTID "@mozilla.org/security/certdownload;1"

//--------------------------------------------
// Now we need a content listener to register 
//--------------------------------------------

class CertContentListener : public nsIURIContentListener {
public:
  CertContentListener();
  virtual ~CertContentListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER
  nsresult init ();
private:
  nsCOMPtr<nsISupports> mLoadCookie;
  nsCOMPtr<nsIURIContentListener> mParentContentListener;
};

// Implementation of the PSM component interface.
class nsPSMComponent : public nsIPSMComponent, 
                       public nsIContentHandler, 
                       public nsISignatureVerifier
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_PSMCOMPONENT_CID );

  nsPSMComponent();
  virtual ~nsPSMComponent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYMANAGERCOMPONENT
  NS_DECL_NSIPSMCOMPONENT
  NS_DECL_NSICONTENTHANDLER
  NS_DECL_NSISIGNATUREVERIFIER

  static NS_METHOD CreatePSMComponent(nsISupports* aOuter, REFNSIID aIID, void **aResult);
  nsresult RegisterCertContentListener();
private:
  
  PCMT_CONTROL mControl;
  
  nsCOMPtr<nsISupports> mSecureBrowserIU;
  nsCOMPtr<nsIURIContentListener> mCertContentListener;
  static nsPSMComponent* mInstance;
}; 

#endif //_NSPSMCOMPONENT_H
