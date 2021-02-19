/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessible_h
#define mozilla_a11y_RemoteAccessible_h

#include "LocalAccessible.h"
#include "mozilla/a11y/RemoteAccessibleBase.h"
#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"

namespace mozilla {
namespace a11y {

class RemoteAccessible : public RemoteAccessibleBase<RemoteAccessible> {
 public:
  RemoteAccessible(uint64_t aID, RemoteAccessible* aParent,
                   DocAccessibleParent* aDoc, role aRole, uint32_t aInterfaces)
      : RemoteAccessibleBase(aID, aParent, aDoc, aRole, aInterfaces)

  {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }

  MOZ_COUNTED_DTOR(RemoteAccessible)

#include "mozilla/a11y/RemoteAccessibleShared.h"

 protected:
  explicit RemoteAccessible(DocAccessibleParent* aThisAsDoc)
      : RemoteAccessibleBase(aThisAsDoc) {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }
};

}  // namespace a11y
}  // namespace mozilla

#endif
