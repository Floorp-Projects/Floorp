/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadioRequestParent.h"
#include "FMRadioService.h"
#include "mozilla/unused.h"
#include "mozilla/dom/PFMRadio.h"

BEGIN_FMRADIO_NAMESPACE

FMRadioRequestParent::FMRadioRequestParent()
  : mActorDestroyed(false)
{
  MOZ_COUNT_CTOR(FMRadioRequestParent);
}

FMRadioRequestParent::~FMRadioRequestParent()
{
  MOZ_COUNT_DTOR(FMRadioRequestParent);
}

void
FMRadioRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
}

nsresult
FMRadioRequestParent::Run()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (!mActorDestroyed) {
    unused << Send__delete__(this, mResponseType);
  }

  return NS_OK;
}

END_FMRADIO_NAMESPACE

