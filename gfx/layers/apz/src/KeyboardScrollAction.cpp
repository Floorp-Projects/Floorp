/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/KeyboardScrollAction.h"

namespace mozilla {
namespace layers {

/* static */ nsIScrollableFrame::ScrollUnit
KeyboardScrollAction::GetScrollUnit(KeyboardScrollAction::KeyboardScrollActionType aDeltaType)
{
  switch (aDeltaType) {
    case KeyboardScrollAction::eScrollCharacter:
      return nsIScrollableFrame::LINES;
    case KeyboardScrollAction::eScrollLine:
      return nsIScrollableFrame::LINES;
    case KeyboardScrollAction::eScrollPage:
      return nsIScrollableFrame::PAGES;
    case KeyboardScrollAction::eScrollComplete:
      return nsIScrollableFrame::WHOLE;
    case KeyboardScrollAction::eSentinel:
      MOZ_ASSERT_UNREACHABLE("Invalid KeyboardScrollActionType.");
      return nsIScrollableFrame::WHOLE;
  }

  // Silence an overzealous warning
  return nsIScrollableFrame::WHOLE;
}

KeyboardScrollAction::KeyboardScrollAction()
  : mType(KeyboardScrollAction::eScrollCharacter)
  , mForward(false)
{
}

KeyboardScrollAction::KeyboardScrollAction(KeyboardScrollActionType aType, bool aForward)
  : mType(aType)
  , mForward(aForward)
{
}

} // namespace layers
} // namespace mozilla
