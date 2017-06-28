/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoRestyleTimelineMarker_h_
#define mozilla_AutoRestyleTimelineMarker_h_

#include "mozilla/GuardObjects.h"
#include "mozilla/RefPtr.h"

class nsIDocShell;

namespace mozilla {

class MOZ_RAII AutoRestyleTimelineMarker
{
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

  RefPtr<nsIDocShell> mDocShell;
  bool mIsAnimationOnly;

public:
  AutoRestyleTimelineMarker(nsIDocShell* aDocShell,
                            bool aIsAnimationOnly
                            MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~AutoRestyleTimelineMarker();

  AutoRestyleTimelineMarker(const AutoRestyleTimelineMarker& aOther) = delete;
  void operator=(const AutoRestyleTimelineMarker& aOther) = delete;
};

} // namespace mozilla

#endif /* mozilla_AutoRestyleTimelineMarker_h_ */
