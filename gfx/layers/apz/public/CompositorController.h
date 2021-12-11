/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorController_h
#define mozilla_layers_CompositorController_h

#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
#include "mozilla/Maybe.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

class CompositorController {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  /**
   * Ask the compositor to schedule a new composite.
   */
  virtual void ScheduleRenderOnCompositorThread(wr::RenderReasons aReasons) = 0;

 protected:
  virtual ~CompositorController() = default;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorController_h
