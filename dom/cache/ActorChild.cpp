/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ActorChild.h"

#include "mozilla/dom/cache/Feature.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

void
ActorChild::SetFeature(Feature* aFeature)
{
  // Some of the Cache actors can have multiple DOM objects associated with
  // them.  In this case the feature will be added multiple times.  This is
  // permitted, but the feature should be the same each time.
  if (mFeature) {
    MOZ_ASSERT(mFeature == aFeature);
    return;
  }

  mFeature = aFeature;
  if (mFeature) {
    mFeature->AddActor(this);
  }
}

void
ActorChild::RemoveFeature()
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), mFeature);
  if (mFeature) {
    mFeature->RemoveActor(this);
    mFeature = nullptr;
  }
}

Feature*
ActorChild::GetFeature() const
{
  return mFeature;
}

bool
ActorChild::FeatureNotified() const
{
  return mFeature && mFeature->Notified();
}

ActorChild::ActorChild()
{
}

ActorChild::~ActorChild()
{
  MOZ_ASSERT(!mFeature);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
