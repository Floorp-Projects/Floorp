/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridgeParent_h
#define mozilla_layers_APZInputBridgeParent_h

#include "mozilla/layers/PAPZInputBridgeParent.h"

namespace mozilla {
namespace layers {

class IAPZCTreeManager;

class APZInputBridgeParent : public PAPZInputBridgeParent
{
  NS_INLINE_DECL_REFCOUNTING(APZInputBridgeParent)

public:
  explicit APZInputBridgeParent(const uint64_t& aLayersId);

  void
  ActorDestroy(ActorDestroyReason aWhy) override;

protected:
  virtual ~APZInputBridgeParent();

private:
  RefPtr<IAPZCTreeManager> mTreeManager;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZInputBridgeParent_h
