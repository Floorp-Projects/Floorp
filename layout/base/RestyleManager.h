/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code responsible for managing style changes: tracking what style
 * changes need to happen, scheduling them, and doing them.
 */

#ifndef mozilla_RestyleManager_h
#define mozilla_RestyleManager_h

#include "nsISupportsImpl.h"

class nsPresContext;

namespace mozilla {

class RestyleManager {
public:
  RestyleManager(nsPresContext* aPresContext);

  NS_INLINE_DECL_REFCOUNTING(mozilla::RestyleManager)

  void Disconnect() {
    mPresContext = nullptr;
  }

  nsPresContext* PresContext() const {
    MOZ_ASSERT(mPresContext);
    return mPresContext;
  }

private:
  nsPresContext* mPresContext; // weak, disconnected in Disconnect
};

} // namespace mozilla

#endif /* mozilla_RestyleManager_h */
