/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_nsIRemoteBlob_h
#define mozilla_dom_ipc_nsIRemoteBlob_h

#include "nsISupports.h"

#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

#define NS_IREMOTEBLOB_IID \
  {0x74ce3cdd, 0xbfc9, 0x4edb, {0x98, 0x26, 0x50, 0xcf, 0x00, 0x26, 0x58, 0x70}}

class NS_NO_VTABLE nsIRemoteBlob : public nsISupports
{
public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IREMOTEBLOB_IID)

  // This will either return a PBlobChild or PBlobParent.
  virtual void* GetPBlob() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRemoteBlob, NS_IREMOTEBLOB_IID)

#endif // mozilla_dom_ipc_nsIRemoteBlob_h
