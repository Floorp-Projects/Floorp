/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyParent_h
#define mozilla_dom_telephony_TelephonyParent_h

#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "mozilla/dom/telephony/PTelephonyParent.h"
#include "mozilla/dom/telephony/PTelephonyRequestParent.h"
#include "nsITelephonyProvider.h"

BEGIN_TELEPHONY_NAMESPACE

class TelephonyParent : public PTelephonyParent
                      , public nsITelephonyListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

  TelephonyParent();

protected:
  virtual ~TelephonyParent() {}

  virtual void
  ActorDestroy(ActorDestroyReason why);

  virtual bool
  RecvPTelephonyRequestConstructor(PTelephonyRequestParent* aActor) MOZ_OVERRIDE;

  virtual PTelephonyRequestParent*
  AllocPTelephonyRequestParent() MOZ_OVERRIDE;

  virtual bool
  DeallocPTelephonyRequestParent(PTelephonyRequestParent* aActor) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvRegisterListener() MOZ_OVERRIDE;

  virtual bool
  RecvUnregisterListener() MOZ_OVERRIDE;

  virtual bool
  RecvDialCall(const uint32_t& aClientId, const nsString& aNumber, const bool& aIsEmergency) MOZ_OVERRIDE;

  virtual bool
  RecvHangUpCall(const uint32_t& aClientId, const uint32_t& aCallIndex) MOZ_OVERRIDE;

  virtual bool
  RecvAnswerCall(const uint32_t& aClientId, const uint32_t& aCallIndex) MOZ_OVERRIDE;

  virtual bool
  RecvRejectCall(const uint32_t& aClientId, const uint32_t& aCallIndex) MOZ_OVERRIDE;

  virtual bool
  RecvHoldCall(const uint32_t& aClientId, const uint32_t& aCallIndex) MOZ_OVERRIDE;

  virtual bool
  RecvResumeCall(const uint32_t& aClientId, const uint32_t& aCallIndex) MOZ_OVERRIDE;

  virtual bool
  RecvConferenceCall(const uint32_t& aClientId) MOZ_OVERRIDE;

  virtual bool
  RecvSeparateCall(const uint32_t& aClientId, const uint32_t& callIndex) MOZ_OVERRIDE;

  virtual bool
  RecvHoldConference(const uint32_t& aClientId) MOZ_OVERRIDE;

  virtual bool
  RecvResumeConference(const uint32_t& aClientId) MOZ_OVERRIDE;

  virtual bool
  RecvStartTone(const uint32_t& aClientId, const nsString& aTone) MOZ_OVERRIDE;

  virtual bool
  RecvStopTone(const uint32_t& aClientId) MOZ_OVERRIDE;

  virtual bool
  RecvGetMicrophoneMuted(bool* aMuted) MOZ_OVERRIDE;

  virtual bool
  RecvSetMicrophoneMuted(const bool& aMuted) MOZ_OVERRIDE;

  virtual bool
  RecvGetSpeakerEnabled(bool* aEnabled) MOZ_OVERRIDE;

  virtual bool
  RecvSetSpeakerEnabled(const bool& aEnabled) MOZ_OVERRIDE;

private:
  bool mActorDestroyed;
  bool mRegistered;
};

class TelephonyRequestParent : public PTelephonyRequestParent
                             , public nsITelephonyListener
{
  friend class TelephonyParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

protected:
  TelephonyRequestParent();
  virtual ~TelephonyRequestParent() {}

  virtual void
  ActorDestroy(ActorDestroyReason why);

private:
  bool mActorDestroyed;

  bool
  DoRequest();
};

END_TELEPHONY_NAMESPACE

#endif /* mozilla_dom_telephony_TelephonyParent_h */
