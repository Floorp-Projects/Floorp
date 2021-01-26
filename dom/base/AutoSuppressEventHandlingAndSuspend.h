/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_base_AutoSuppressEventHandlingAndSuspend_h
#define dom_base_AutoSuppressEventHandlingAndSuspend_h

#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"

namespace mozilla::dom {

class BrowsingContext;
class BrowsingContextGroup;

/**
 * Suppresses event handling and suspends the active inner window for all
 * in-process documents in a BrowsingContextGroup. This should be used while
 * spinning the event loop for a synchronous operation (like `window.open()`)
 * which affects operations in any other window in the same BrowsingContext
 * group.
 */

class MOZ_RAII AutoSuppressEventHandlingAndSuspend {
 public:
  explicit AutoSuppressEventHandlingAndSuspend(BrowsingContextGroup* aGroup);
  ~AutoSuppressEventHandlingAndSuspend();

 private:
  void SuppressBrowsingContext(BrowsingContext* aBC);

  AutoTArray<RefPtr<Document>, 16> mDocuments;
  AutoTArray<nsCOMPtr<nsPIDOMWindowInner>, 16> mWindows;
};
}  // namespace mozilla::dom

#endif
