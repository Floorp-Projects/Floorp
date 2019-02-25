/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsIContentParent_h
#define mozilla_dom_nsIContentParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"

#include "nsFrameMessageManager.h"
#include "nsISupports.h"
#include "mozilla/dom/CPOWManagerGetter.h"

#define NS_ICONTENTPARENT_IID                        \
  {                                                  \
    0xeeec9ebf, 0x8ecf, 0x4e38, {                    \
      0x81, 0xda, 0xb7, 0x34, 0x13, 0x7e, 0xac, 0xf3 \
    }                                                \
  }

namespace IPC {
class Principal;
}  // namespace IPC

namespace mozilla {

namespace jsipc {
class PJavaScriptParent;
class CpowEntry;
}  // namespace jsipc

namespace ipc {
class PChildToParentStreamParent;
class PParentToChildStreamParent;
class PIPCBlobInputStreamParent;
}  // namespace ipc

namespace dom {

class Blob;
class BlobConstructorParams;
class BlobImpl;
class ProcessMessageManager;
class ContentParent;
class IPCTabContext;
class PBrowserParent;

class nsIContentParent : public nsISupports,
                         public mozilla::dom::ipc::MessageManagerCallback,
                         public CPOWManagerGetter,
                         public mozilla::ipc::IShmemAllocator {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTPARENT_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentParent, NS_ICONTENTPARENT_IID)

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_nsIContentParent_h */
