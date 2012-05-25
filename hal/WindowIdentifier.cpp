/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "WindowIdentifier.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace hal {

WindowIdentifier::WindowIdentifier()
  : mWindow(nsnull)
  , mIsEmpty(true)
{
}

WindowIdentifier::WindowIdentifier(nsIDOMWindow *window)
  : mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const nsTArray<uint64_t> &id, nsIDOMWindow *window)
  : mID(id)
  , mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const WindowIdentifier &other)
  : mID(other.mID)
  , mWindow(other.mWindow)
  , mIsEmpty(other.mIsEmpty)
{
}

const InfallibleTArray<uint64_t>&
WindowIdentifier::AsArray() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID;
}

bool
WindowIdentifier::HasTraveledThroughIPC() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID.Length() >= 2;
}

void
WindowIdentifier::AppendProcessID()
{
  MOZ_ASSERT(!mIsEmpty);
  mID.AppendElement(dom::ContentChild::GetSingleton()->GetID());
}

uint64_t
WindowIdentifier::GetWindowID() const
{
  MOZ_ASSERT(!mIsEmpty);
  nsCOMPtr<nsPIDOMWindow> pidomWindow = do_QueryInterface(mWindow);
  if (!pidomWindow) {
    return UINT64_MAX;
  }
  return pidomWindow->WindowID();
}

nsIDOMWindow*
WindowIdentifier::GetWindow() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mWindow;
}

} // namespace hal
} // namespace mozilla
