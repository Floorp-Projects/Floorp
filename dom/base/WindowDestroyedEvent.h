/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowDestroyedEvent_h
#define WindowDestroyedEvent_h

#include "nsIWeakReferenceUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

class nsGlobalWindowInner;
class nsGlobalWindowOuter;

namespace mozilla {

class WindowDestroyedEvent final : public Runnable {
 public:
  WindowDestroyedEvent(nsGlobalWindowInner* aWindow, uint64_t aID,
                       const char* aTopic);
  WindowDestroyedEvent(nsGlobalWindowOuter* aWindow, uint64_t aID,
                       const char* aTopic);

  enum class Phase { Destroying, Nuking };

  NS_IMETHOD Run() override;

 private:
  uint64_t mID;
  Phase mPhase;
  nsCString mTopic;
  nsWeakPtr mWindow;
  bool mIsInnerWindow;
};

}  // namespace mozilla

#endif  // defined(WindowDestroyedEvent_h)
