/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_KeyboardScrollAction_h
#define mozilla_layers_KeyboardScrollAction_h

#include "mozilla/DefineEnum.h" // for MOZ_DEFINE_ENUM
#include "nsIScrollableFrame.h" // for nsIScrollableFrame::ScrollUnit

namespace mozilla {
namespace layers {

/**
 * This class represents a scrolling action to be performed on a scrollable layer.
 */
struct KeyboardScrollAction final
{
public:
  MOZ_DEFINE_ENUM_WITH_BASE_AT_CLASS_SCOPE(
    KeyboardScrollActionType, uint8_t, (
      eScrollCharacter,
      eScrollLine,
      eScrollPage,
      eScrollComplete
  ));

  static nsIScrollableFrame::ScrollUnit
  GetScrollUnit(KeyboardScrollActionType aDeltaType);

  KeyboardScrollAction();
  KeyboardScrollAction(KeyboardScrollActionType aType, bool aForward);

  // The type of scroll to perform for this action
  KeyboardScrollActionType mType;
  // Whether to scroll forward or backward along the axis of this action type
  bool mForward;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_KeyboardScrollAction_h
