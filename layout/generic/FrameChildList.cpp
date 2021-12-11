/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layout/FrameChildList.h"

#include "nsIFrame.h"

namespace mozilla {
namespace layout {

#ifdef DEBUG_FRAME_DUMP
const char* ChildListName(FrameChildListID aListID) {
  switch (aListID) {
    case kPrincipalList:
      return "";
    case kPopupList:
      return "PopupList";
    case kCaptionList:
      return "CaptionList";
    case kColGroupList:
      return "ColGroupList";
    case kSelectPopupList:
      return "SelectPopupList";
    case kAbsoluteList:
      return "AbsoluteList";
    case kFixedList:
      return "FixedList";
    case kOverflowList:
      return "OverflowList";
    case kOverflowContainersList:
      return "OverflowContainersList";
    case kExcessOverflowContainersList:
      return "ExcessOverflowContainersList";
    case kOverflowOutOfFlowList:
      return "OverflowOutOfFlowList";
    case kFloatList:
      return "FloatList";
    case kBulletList:
      return "BulletList";
    case kPushedFloatsList:
      return "PushedFloatsList";
    case kBackdropList:
      return "BackdropList";
    case kNoReflowPrincipalList:
      return "NoReflowPrincipalList";
  }

  MOZ_ASSERT_UNREACHABLE("unknown list");
  return "UNKNOWN_FRAME_CHILD_LIST";
}
#endif

}  // namespace layout
}  // namespace mozilla
