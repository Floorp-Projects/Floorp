/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PFMRadioRequestChild.h"
#include "FMRadioRequestChild.h"
#include "FMRadioService.h"

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestChild::FMRadioRequestChild(FMRadioReplyRunnable* aReplyRunnable)
  : mReplyRunnable(aReplyRunnable)
{
  MOZ_COUNT_CTOR(FMRadioRequestChild);
}

FMRadioRequestChild::~FMRadioRequestChild()
{
  MOZ_COUNT_DTOR(FMRadioRequestChild);
}

bool
FMRadioRequestChild::Recv__delete__(const FMRadioResponseType& aType)
{
  mReplyRunnable->SetReply(aType);
  NS_DispatchToMainThread(mReplyRunnable);

  return true;
}

END_FMRADIO_NAMESPACE

