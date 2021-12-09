/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICanvasRenderingContextInternal.h"

#include "mozilla/dom/Document.h"
#include "mozilla/PresShell.h"
#include "nsRefreshDriver.h"

nsICanvasRenderingContextInternal::nsICanvasRenderingContextInternal()
    : mSharedPtrPtr(
          std::make_shared<nsICanvasRenderingContextInternal*>(this)) {}

nsICanvasRenderingContextInternal::~nsICanvasRenderingContextInternal() =
    default;

mozilla::PresShell* nsICanvasRenderingContextInternal::GetPresShell() {
  if (mCanvasElement) {
    return mCanvasElement->OwnerDoc()->GetPresShell();
  }
  return nullptr;
}

void nsICanvasRenderingContextInternal::RemovePostRefreshObserver() {
  if (mRefreshDriver) {
    mRefreshDriver->RemovePostRefreshObserver(this);
    mRefreshDriver = nullptr;
  }
}

void nsICanvasRenderingContextInternal::AddPostRefreshObserverIfNecessary() {
  if (!GetPresShell() || !GetPresShell()->GetPresContext() ||
      !GetPresShell()->GetPresContext()->RefreshDriver()) {
    return;
  }
  mRefreshDriver = GetPresShell()->GetPresContext()->RefreshDriver();
  mRefreshDriver->AddPostRefreshObserver(this);
}
