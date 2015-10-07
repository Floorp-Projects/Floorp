/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyParent_h
#define mozilla_dom_telephony_TelephonyParent_h

#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "mozilla/dom/telephony/PTelephonyParent.h"
#include "mozilla/dom/telephony/PTelephonyRequestParent.h"
#include "nsITelephonyService.h"

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
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvPTelephonyRequestConstructor(PTelephonyRequestParent* aActor, const IPCTelephonyRequest& aRequest) override;

  virtual PTelephonyRequestParent*
  AllocPTelephonyRequestParent(const IPCTelephonyRequest& aRequest) override;

  virtual bool
  DeallocPTelephonyRequestParent(PTelephonyRequestParent* aActor) override;

  virtual bool
  Recv__delete__() override;

  virtual bool
  RecvRegisterListener() override;

  virtual bool
  RecvUnregisterListener() override;

  virtual bool
  RecvStartTone(const uint32_t& aClientId, const nsString& aTone) override;

  virtual bool
  RecvStopTone(const uint32_t& aClientId) override;

  virtual bool
  RecvGetMicrophoneMuted(bool* aMuted) override;

  virtual bool
  RecvSetMicrophoneMuted(const bool& aMuted) override;

  virtual bool
  RecvGetSpeakerEnabled(bool* aEnabled) override;

  virtual bool
  RecvSetSpeakerEnabled(const bool& aEnabled) override;

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

  class Callback : public nsITelephonyCallback {
    friend class TelephonyRequestParent;

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITELEPHONYCALLBACK

  protected:
    explicit Callback(TelephonyRequestParent& aParent): mParent(aParent) {}
    virtual ~Callback() {}

  private:
    nsresult SendResponse(const IPCTelephonyResponse& aResponse);
    TelephonyRequestParent& mParent;
  };

  class DialCallback final : public Callback
                           , public nsITelephonyDialCallback {
    friend class TelephonyRequestParent;

  public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSITELEPHONYDIALCALLBACK
    NS_FORWARD_NSITELEPHONYCALLBACK(Callback::)

  private:
    explicit DialCallback(TelephonyRequestParent& aParent): Callback(aParent) {}
    ~DialCallback() {}
  };

protected:
  TelephonyRequestParent();
  virtual ~TelephonyRequestParent() {}

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult
  SendResponse(const IPCTelephonyResponse& aResponse);

  Callback*
  GetCallback() {
    return mCallback;
  }

  DialCallback*
  GetDialCallback() {
    return mDialCallback;
  }

private:
  bool mActorDestroyed;
  nsRefPtr<Callback> mCallback;
  nsRefPtr<DialCallback> mDialCallback;
};

END_TELEPHONY_NAMESPACE

#endif /* mozilla_dom_telephony_TelephonyParent_h */
