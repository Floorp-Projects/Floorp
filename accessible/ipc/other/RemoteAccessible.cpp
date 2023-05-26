/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "AccAttributes.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "Relation.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "nsAccessibilityService.h"

namespace mozilla {
namespace a11y {

void RemoteAccessible::ScrollToPoint(uint32_t aScrollType, int32_t aX,
                                     int32_t aY) {
  Unused << mDoc->SendScrollToPoint(mID, aScrollType, aX, aY);
}

void RemoteAccessible::Announce(const nsString& aAnnouncement,
                                uint16_t aPriority) {
  Unused << mDoc->SendAnnounce(mID, aAnnouncement, aPriority);
}

void RemoteAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                              int32_t aEndOffset,
                                              uint32_t aCoordinateType,
                                              int32_t aX, int32_t aY) {
  Unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

}  // namespace a11y
}  // namespace mozilla
