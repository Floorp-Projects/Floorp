/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InputAPZContext_h
#define mozilla_layers_InputAPZContext_h

#include "FrameMetrics.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace layers {

// InputAPZContext is used to communicate various pieces of information
// around the codebase without having to plumb it through lots of functions
// and codepaths. Conceptually it is attached to a WidgetInputEvent that is
// relevant to APZ.
//
// There are two types of information bits propagated using this class. One
// type is propagated "downwards" (from a process entry point like nsBaseWidget
// or TabChild) into deeper code that is run during complicated operations
// like event dispatch. The other type is information that is propagated
// "upwards", from the deeper code back to the entry point.
class MOZ_STACK_CLASS InputAPZContext
{
private:
  // State that is propagated downwards from InputAPZContext creation into
  // "deeper" code.
  static ScrollableLayerGuid sGuid;
  static uint64_t sBlockId;
  static nsEventStatus sApzResponse;
  static bool sPendingLayerization;

  // State that is set in deeper code and propagated upwards.
  static bool sRoutedToChildProcess;

public:
  // Functions to access downwards-propagated data
  static ScrollableLayerGuid GetTargetLayerGuid();
  static uint64_t GetInputBlockId();
  static nsEventStatus GetApzResponse();
  static bool HavePendingLayerization();

  // Functions to access upwards-propagated data
  static bool WasRoutedToChildProcess();

  // Constructor sets the data to be propagated downwards
  InputAPZContext(const ScrollableLayerGuid& aGuid,
                  const uint64_t& aBlockId,
                  const nsEventStatus& aApzResponse,
                  bool aPendingLayerization = false);
  ~InputAPZContext();

  // Functions to set data to be propagated upwards
  static void SetRoutedToChildProcess();

private:
  ScrollableLayerGuid mOldGuid;
  uint64_t mOldBlockId;
  nsEventStatus mOldApzResponse;
  bool mOldPendingLayerization;

  bool mOldRoutedToChildProcess;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_InputAPZContext_h */
