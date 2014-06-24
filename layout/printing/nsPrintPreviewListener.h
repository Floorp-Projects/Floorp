/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintPreviewListener_h__
#define nsPrintPreviewListener_h__

// Interfaces needed to be included
#include "nsIDOMEventListener.h"
// Helper Classes
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
class EventTarget;
}
}

//
// class nsPrintPreviewListener
//
// The class that listens to the chrome events and tells the embedding
// chrome to show context menus, as appropriate. Handles registering itself
// with the DOM with AddChromeListeners() and removing itself with
// RemoveChromeListeners().
//
class nsPrintPreviewListener MOZ_FINAL : public nsIDOMEventListener

{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsPrintPreviewListener(mozilla::dom::EventTarget* aTarget);

  // Add/remove the relevant listeners, based on what interfaces
  // the embedding chrome implements.
  nsresult AddListeners();
  nsresult RemoveListeners();

private:
  ~nsPrintPreviewListener();

  nsCOMPtr<mozilla::dom::EventTarget> mEventTarget;

}; // class nsPrintPreviewListener



#endif /* nsPrintPreviewListener_h__ */
