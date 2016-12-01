/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  {0x0b8b0091, 0xb315, 0x48a2, {0x90, 0xf1, 0x60, 0x0e, 0x78, 0x35, 0xf7, 0x2d}}

namespace mozilla {
namespace dom {

class BlobChild;
class BlobParent;

} // namespace dom
} // namespace mozilla

class NS_NO_VTABLE nsIRemoteBlob : public nsISupports
{
public:
  typedef mozilla::dom::BlobChild BlobChild;
  typedef mozilla::dom::BlobParent BlobParent;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IREMOTEBLOB_IID)

  virtual BlobChild*
  GetBlobChild() = 0;

  virtual BlobParent*
  GetBlobParent() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRemoteBlob, NS_IREMOTEBLOB_IID)

#endif // mozilla_dom_ipc_nsIRemoteBlob_h
