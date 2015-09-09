/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MutableFileBase.h"

#include "ActorsChild.h"
#include "mozilla/Assertions.h"
#include "prthread.h"

namespace mozilla {
namespace dom {

MutableFileBase::MutableFileBase(DEBUGONLY(PRThread* aOwningThread,)
                                 BackgroundMutableFileChildBase* aActor)
  : RefCountedThreadObject(DEBUGONLY(aOwningThread))
  , mBackgroundActor(aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
}

MutableFileBase::~MutableFileBase()
{
  AssertIsOnOwningThread();

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

} // namespace dom
} // namespace mozilla
