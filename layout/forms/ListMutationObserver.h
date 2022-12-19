/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ListMutationObserver_h
#define mozilla_ListMutationObserver_h

#include "IDTracker.h"
#include "nsStubMutationObserver.h"

class nsIFrame;

namespace mozilla {

namespace dom {
class HTMLInputElement;
}  // namespace dom

/**
 * ListMutationObserver
 * This class invalidates paint for the input element's frame when the content
 * of its @list changes, or when the @list ID identifies a different element. It
 * does *not* invalidate paint when the @list attribute itself changes.
 */

class ListMutationObserver final : public nsStubMutationObserver,
                                   public dom::IDTracker {
 public:
  explicit ListMutationObserver(nsIFrame& aOwningElementFrame,
                                bool aRepaint = false)
      : mOwningElementFrame(&aOwningElementFrame) {
    // We can skip invalidating paint if the frame is still being initialized.
    Attach(aRepaint);
  }

  NS_DECL_ISUPPORTS

  // We need to invalidate paint when the list or its options change.
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  /**
   * Triggered when the same @list ID identifies a different element than
   * before.
   */
  void ElementChanged(dom::Element* aFrom, dom::Element* aTo) override;

  void Attach(bool aRepaint = true);
  void Detach();
  void AddObserverIfNeeded();
  void RemoveObserverIfNeeded(dom::Element* aList);
  void RemoveObserverIfNeeded() { RemoveObserverIfNeeded(get()); }
  dom::HTMLInputElement& InputElement() const;

 private:
  ~ListMutationObserver();

  nsIFrame* mOwningElementFrame;
};
}  // namespace mozilla

#endif  // mozilla_ListMutationObserver_h
