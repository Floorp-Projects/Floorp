/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InProcessParent.h"
#include "mozilla/dom/WindowGlobalParent.h"

using namespace mozilla::dom;

namespace mozilla {
namespace ipc {

NS_IMPL_ISUPPORTS(InProcessParent, nsIObserver)

IPCResult
InProcessParent::RecvPWindowGlobalConstructor(PWindowGlobalParent* aActor,
                                              const WindowGlobalInit& aInit)
{
  static_cast<WindowGlobalParent*>(aActor)->Init(aInit);
  return IPC_OK();
}

PWindowGlobalParent*
InProcessParent::AllocPWindowGlobalParent(const WindowGlobalInit& aInit)
{
  // Reference freed in DeallocPWindowGlobalParent.
  return do_AddRef(new WindowGlobalParent(aInit, /* inproc */ true)).take();
}

bool
InProcessParent::DeallocPWindowGlobalParent(PWindowGlobalParent* aActor)
{
  // Free IPC-held reference.
  static_cast<WindowGlobalParent*>(aActor)->Release();
  return true;
}

} // namespace ipc
} // namespace mozilla