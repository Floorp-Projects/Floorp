/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRefreshDriver.h"
#include "BrowserChild.h"
#include "CoalescedInputData.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using namespace mozilla::dom;

void CoalescedInputFlusher::StartObserver() {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (mRefreshDriver && mRefreshDriver == refreshDriver) {
    // Nothing to do if we already added an observer and it's same refresh
    // driver.
    return;
  }
  RemoveObserver();
  if (refreshDriver) {
    mRefreshDriver = refreshDriver;
    mRefreshDriver->AddRefreshObserver(this, FlushType::Event,
                                       "Coalesced input move flusher");
  }
}

CoalescedInputFlusher::CoalescedInputFlusher(BrowserChild* aBrowserChild)
    : mBrowserChild(aBrowserChild) {}

void CoalescedInputFlusher::RemoveObserver() {
  if (mRefreshDriver) {
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Event);
    mRefreshDriver = nullptr;
  }
}

CoalescedInputFlusher::~CoalescedInputFlusher() = default;

nsRefreshDriver* CoalescedInputFlusher::GetRefreshDriver() {
  if (PresShell* presShell = mBrowserChild->GetTopLevelPresShell()) {
    return presShell->GetRefreshDriver();
  }
  return nullptr;
}
