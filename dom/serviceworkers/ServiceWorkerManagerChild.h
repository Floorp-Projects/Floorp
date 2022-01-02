/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerManagerChild_h
#define mozilla_dom_ServiceWorkerManagerChild_h

#include "mozilla/dom/PServiceWorkerManagerChild.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla {

class OriginAttributes;

namespace ipc {
class BackgroundChildImpl;
}  // namespace ipc

namespace dom {

class ServiceWorkerManagerChild final : public PServiceWorkerManagerChild {
  friend class mozilla::ipc::BackgroundChildImpl;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerManagerChild)

  void ManagerShuttingDown() { mShuttingDown = true; }

 private:
  ServiceWorkerManagerChild() : mShuttingDown(false) {}

  ~ServiceWorkerManagerChild() = default;

  bool mShuttingDown;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ServiceWorkerManagerChild_h
