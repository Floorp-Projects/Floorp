/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorVsyncSchedulerOwner_h
#define mozilla_layers_CompositorVsyncSchedulerOwner_h

namespace mozilla {

namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class CompositorVsyncSchedulerOwner
{
public:
  virtual bool IsPendingComposite() = 0;
  virtual void FinishPendingComposite() = 0;
  virtual void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr) = 0;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorVsyncSchedulerOwner_h
