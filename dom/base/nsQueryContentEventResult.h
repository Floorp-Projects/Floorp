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
#include "Units.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"

class nsIWidget;

class nsQueryContentEventResult final : public nsIQueryContentEventResult
{
public:
  nsQueryContentEventResult();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUERYCONTENTEVENTRESULT

  void SetEventResult(nsIWidget* aWidget,
                      mozilla::WidgetQueryContentEvent& aEvent);

protected:
  ~nsQueryContentEventResult();

  mozilla::EventMessage mEventMessage;

  uint32_t mOffset;
  uint32_t mTentativeCaretOffset;
  nsString mString;
  mozilla::LayoutDeviceIntRect mRect;
  nsTArray<mozilla::LayoutDeviceIntRect> mRectArray;

  bool mSucceeded;
  bool mReversed;
};

#endif // mozilla_dom_nsQueryContentEventResult_h
