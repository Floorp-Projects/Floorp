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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
*/

#ifndef _NSTLSSOCKETPROVIDER_H_
#define _NSTLSSOCKETPROVIDER_H_

#include "nsISSLSocketProvider.h"

/* 274418d0-5437-11d3-bbc8-0000861d1237 */         
#define NS_TLSSOCKETPROVIDER_CID \
{ /* 88f2df38-1dd2-11b2-97fd-ac6da6bfab7f */         \
     0x88f2df38,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x97, 0xfd, 0xac, 0x6d, 0xa6, 0xbf, 0xab, 0x7f} \
}

class nsTLSSocketProvider : public nsISSLSocketProvider
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSISOCKETPROVIDER

  NS_DECL_NSISSLSOCKETPROVIDER

  // nsTLSSocketProvider methods:
  nsTLSSocketProvider();
  virtual ~nsTLSSocketProvider();

  static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  nsresult Init();

protected:
};

#endif /* _NSTLSSOCKETPROVIDER_H_ */
