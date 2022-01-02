/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsQueryContentEventResult_h
#define mozilla_dom_nsQueryContentEventResult_h

#include "nsIQueryContentEventResult.h"
#include "nsString.h"
#include "nsRect.h"
#include "nsTArray.h"
#include "Units.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/widget/IMEData.h"

class nsIWidget;

class nsQueryContentEventResult final : public nsIQueryContentEventResult {
 public:
  explicit nsQueryContentEventResult(mozilla::WidgetQueryContentEvent&& aEvent);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUERYCONTENTEVENTRESULT

  void SetEventResult(nsIWidget* aWidget);

 protected:
  ~nsQueryContentEventResult() = default;

  mozilla::EventMessage mEventMessage;

  mozilla::Maybe<mozilla::OffsetAndData<uint32_t>> mOffsetAndData;
  mozilla::Maybe<uint32_t> mTentativeCaretOffset;
  mozilla::LayoutDeviceIntRect mRect;
  CopyableTArray<mozilla::LayoutDeviceIntRect> mRectArray;

  bool mSucceeded;
  bool mReversed;
};

#endif  // mozilla_dom_nsQueryContentEventResult_h
