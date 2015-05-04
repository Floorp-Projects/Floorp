/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradioparent_h__
#define mozilla_dom_fmradioparent_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/PFMRadioParent.h"
#include "mozilla/HalTypes.h"

BEGIN_FMRADIO_NAMESPACE

class PFMRadioRequestParent;

class FMRadioParent final : public PFMRadioParent
                          , public FMRadioEventObserver
{
public:
  FMRadioParent();
  ~FMRadioParent();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvGetStatusInfo(StatusInfo* aStatusInfo) override;

  virtual PFMRadioRequestParent*
  AllocPFMRadioRequestParent(const FMRadioRequestArgs& aArgs) override;

  virtual bool
  DeallocPFMRadioRequestParent(PFMRadioRequestParent* aActor) override;

  /* FMRadioEventObserver */
  virtual void Notify(const FMRadioEventType& aType) override;

  virtual bool
  RecvEnableAudio(const bool& aAudioEnabled) override;

  virtual bool
  RecvSetRDSGroupMask(const uint32_t& aRDSGroupMask) override;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradioparent_h__

