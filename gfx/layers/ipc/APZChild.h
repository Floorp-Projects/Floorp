/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZChild_h
#define mozilla_layers_APZChild_h

#include "mozilla/layers/PAPZChild.h"

namespace mozilla {

namespace layers {

class GeckoContentController;

/**
 * APZChild implements PAPZChild and is used to remote a GeckoContentController
 * that lives in a different process than where APZ lives.
 */
class APZChild final : public PAPZChild
{
public:
  explicit APZChild(RefPtr<GeckoContentController> aController);
  ~APZChild();

  bool RecvRequestContentRepaint(const FrameMetrics& frame) override;

  bool RecvUpdateOverscrollVelocity(const float& aX, const float& aY, const bool& aIsRootContent) override;

  bool RecvUpdateOverscrollOffset(const float& aX, const float& aY, const bool& aIsRootContent) override;

  bool RecvSetScrollingRootContent(const bool& aIsRootContent) override;

  bool RecvNotifyMozMouseScrollEvent(const ViewID& aScrollId,
                                     const nsString& aEvent) override;

  bool RecvNotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                const APZStateChange& aChange,
                                const int& aArg) override;

  bool RecvNotifyFlushComplete() override;

  bool RecvDestroy() override;

private:
  RefPtr<GeckoContentController> mController;
};

} // namespace layers

} // namespace mozilla

#endif // mozilla_layers_APZChild_h
