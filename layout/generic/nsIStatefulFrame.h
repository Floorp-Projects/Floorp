/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for rendering objects whose state is saved in
 * session-history (back-forward navigation)
 */

#ifndef _nsIStatefulFrame_h
#define _nsIStatefulFrame_h

#include "nsContentUtils.h"
#include "nsQueryFrame.h"

namespace mozilla {
class PresState;
}  // namespace mozilla

class nsIStatefulFrame {
 public:
  NS_DECL_QUERYFRAME_TARGET(nsIStatefulFrame)

  // Save the state for this frame.
  virtual mozilla::UniquePtr<mozilla::PresState> SaveState() = 0;

  // Restore the state for this frame from aState
  NS_IMETHOD RestoreState(mozilla::PresState* aState) = 0;

  // Generate a key for this stateful frame
  virtual void GenerateStateKey(nsIContent* aContent,
                                mozilla::dom::Document* aDocument,
                                nsACString& aKey) {
    nsContentUtils::GenerateStateKey(aContent, aDocument, aKey);
  };
};

#endif /* _nsIStatefulFrame_h */
