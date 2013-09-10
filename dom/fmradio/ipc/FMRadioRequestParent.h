/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradiorequestparent_h__
#define mozilla_dom_fmradiorequestparent_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/PFMRadioRequestParent.h"
#include "FMRadioService.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioRequestParent MOZ_FINAL : public PFMRadioRequestParent
                                     , public FMRadioReplyRunnable
{
public:
  FMRadioRequestParent();
  ~FMRadioRequestParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  NS_IMETHOD Run();

private:
  bool mActorDestroyed;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradiorequestparent_h__

