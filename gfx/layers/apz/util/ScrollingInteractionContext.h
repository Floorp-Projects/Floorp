/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ScrollingInteractionContext_h
#define mozilla_layers_ScrollingInteractionContext_h

#include "mozilla/EventForwards.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

namespace mozilla {
namespace layers {

// The ScrollingInteractionContext is used to store minor details of the
// current scrolling interaction on the stack to avoid having to pass them
// though the callstack
class MOZ_STACK_CLASS ScrollingInteractionContext {
 private:
  static bool sScrollingToAnchor;

 public:
  // Functions to access downwards-propagated data
  static bool IsScrollingToAnchor();

  // Constructor sets the data to be propagated downwards
  explicit ScrollingInteractionContext(bool aScrollingToAnchor);

  // Destructor restores the previous state
  ~ScrollingInteractionContext();

 private:
  bool mOldScrollingToAnchor;
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_ScrollingInteractionContext_h */
