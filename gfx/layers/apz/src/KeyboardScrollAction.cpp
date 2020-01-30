/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/KeyboardScrollAction.h"

namespace mozilla {
namespace layers {

/* static */ ScrollUnit KeyboardScrollAction::GetScrollUnit(
    KeyboardScrollAction::KeyboardScrollActionType aDeltaType) {
  switch (aDeltaType) {
    case KeyboardScrollAction::eScrollCharacter:
      return ScrollUnit::LINES;
    case KeyboardScrollAction::eScrollLine:
      return ScrollUnit::LINES;
    case KeyboardScrollAction::eScrollPage:
      return ScrollUnit::PAGES;
    case KeyboardScrollAction::eScrollComplete:
      return ScrollUnit::WHOLE;
  }

  // Silence an overzealous warning
  return ScrollUnit::WHOLE;
}

KeyboardScrollAction::KeyboardScrollAction()
    : mType(KeyboardScrollAction::eScrollCharacter), mForward(false) {}

KeyboardScrollAction::KeyboardScrollAction(KeyboardScrollActionType aType,
                                           bool aForward)
    : mType(aType), mForward(aForward) {}

}  // namespace layers
}  // namespace mozilla
