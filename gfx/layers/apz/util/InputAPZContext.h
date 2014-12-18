/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InputAPZContext_h
#define mozilla_layers_InputAPZContext_h

#include "FrameMetrics.h"

namespace mozilla {
namespace layers {

// InputAPZContext is used to communicate the ScrollableLayerGuid and input
// block ID from nsIWidget to RenderFrameParent. It is conceptually attached
// to any WidgetInputEvent that has been processed by APZ directly from a
// widget.
class MOZ_STACK_CLASS InputAPZContext
{
private:
  static ScrollableLayerGuid sGuid;
  static uint64_t sBlockId;
  static bool sRoutedToChildProcess;

public:
  static ScrollableLayerGuid GetTargetLayerGuid();
  static uint64_t GetInputBlockId();
  static void SetRoutedToChildProcess();

  InputAPZContext(const ScrollableLayerGuid& aGuid,
                    const uint64_t& aBlockId);
  ~InputAPZContext();

  bool WasRoutedToChildProcess();

private:
  ScrollableLayerGuid mOldGuid;
  uint64_t mOldBlockId;
  bool mOldRoutedToChildProcess;
};

}
}

#endif /* mozilla_layers_InputAPZContext_h */
