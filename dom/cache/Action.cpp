/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Action.h"

namespace mozilla {
namespace dom {
namespace cache {

void
Action::CancelOnInitiatingThread()
{
  NS_ASSERT_OWNINGTHREAD(Action);
  // It is possible for cancellation to be duplicated.  For example, an
  // individual Cache could have its Actions canceled and then shutdown
  // could trigger a second action.
  mCanceled = true;
}

Action::Action()
  : mCanceled(false)
{
}

Action::~Action()
{
}

bool
Action::IsCanceled() const
{
  return mCanceled;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
