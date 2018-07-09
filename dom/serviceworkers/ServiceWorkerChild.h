/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerchild_h__
#define mozilla_dom_serviceworkerchild_h__

#include "mozilla/dom/PServiceWorkerChild.h"

namespace mozilla {
namespace dom {

class ServiceWorkerChild final : public PServiceWorkerChild
{
  // PServiceWorkerChild
  void
  ActorDestroy(ActorDestroyReason aReason) override;

public:
  ServiceWorkerChild() = default;
  ~ServiceWorkerChild() = default;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_serviceworkerchild_h__
