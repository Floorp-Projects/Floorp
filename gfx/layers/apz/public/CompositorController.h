/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorController_h
#define mozilla_layers_CompositorController_h

#include "nsISupportsImpl.h" // for NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

namespace mozilla {
namespace layers {

class CompositorController
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void ScheduleRenderOnCompositorThread() = 0;
  virtual void ScheduleHideAllPluginWindows() = 0;
  virtual void ScheduleShowAllPluginWindows() = 0;

protected:
  virtual ~CompositorController() {}
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorController_h
