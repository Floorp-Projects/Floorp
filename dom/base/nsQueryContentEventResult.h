/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIQueryContentEventResult.h"
#include "nsString.h"
#include "nsRect.h"
#include "mozilla/Attributes.h"

class nsQueryContentEvent;
class nsIWidget;

class nsQueryContentEventResult MOZ_FINAL : public nsIQueryContentEventResult
{
public:
  nsQueryContentEventResult();
  ~nsQueryContentEventResult();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUERYCONTENTEVENTRESULT

  void SetEventResult(nsIWidget* aWidget, const nsQueryContentEvent &aEvent);

protected:
  uint32_t mEventID;

  uint32_t mOffset;
  nsString mString;
  nsIntRect mRect;

  bool mSucceeded;
  bool mReversed;
};
