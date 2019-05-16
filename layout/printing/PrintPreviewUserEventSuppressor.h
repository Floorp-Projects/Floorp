/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrintPreviewUserEventSuppressor_h
#define mozilla_PrintPreviewUserEventSuppressor_h

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "mozilla/Attributes.h"

namespace mozilla {

namespace dom {
class EventTarget;
}  // namespace dom

/**
 * A class that filters out certain user events targeted at the given event
 * target (a document).  Intended for use with the Print Preview document to
 * stop users from doing anything that would break printing invariants.  (For
 * example, blocks opening of the context menu, interaction with form controls,
 * content selection, etc.)
 */
class PrintPreviewUserEventSuppressor final : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit PrintPreviewUserEventSuppressor(dom::EventTarget* aTarget);

  /**
   * Must be called before releasing this object in order to break the strong
   * reference cycle between ourselves and the document we're listening to,
   * or else the objects in the cylce will be leaked (since this class does
   * not participate in cycle collection).
   */
  void StopSuppressing() { RemoveListeners(); }

 private:
  ~PrintPreviewUserEventSuppressor() { RemoveListeners(); }

  void AddListeners();
  void RemoveListeners();

  nsCOMPtr<mozilla::dom::EventTarget> mEventTarget;
};

}  // namespace mozilla

#endif  // mozilla_PrintPreviewUserEventSuppressor_h
