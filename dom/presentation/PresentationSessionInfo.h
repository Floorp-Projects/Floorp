/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSessionInfo_h
#define mozilla_dom_PresentationSessionInfo_h

#include "base/process.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h"
#include "nsIPresentationListener.h"
#include "nsIPresentationService.h"
#include "nsIPresentationSessionTransport.h"
#include "nsIPresentationSessionTransportBuilder.h"
#include "nsIServerSocket.h"
#include "nsITimer.h"
#include "nsString.h"
#include "PresentationCallbacks.h"

namespace mozilla {
namespace dom {

class PresentationSessionInfo : public nsIPresentationSessionTransportCallback
                              , public nsIPresentationControlChannelListener
                              , public nsIPresentationSessionTransportBuilderListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTCALLBACK
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTBUILDERLISTENER

  PresentationSessionInfo(const nsAString& aUrl,
                          const nsAString& aSessionId,
                          const uint8_t aRole)
    : mUrl(aUrl)
    , mSessionId(aSessionId)
    , mIsResponderReady(false)
    , mIsTransportReady(false)
    , mState(nsIPresentationSessionListener::STATE_CONNECTING)
    , mReason(NS_OK)
  {
    MOZ_ASSERT(!mUrl.IsEmpty());
    MOZ_ASSERT(!mSessionId.IsEmpty());
    MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
               aRole == nsIPresentationService::ROLE_RECEIVER);
    mRole = aRole;
  }

  virtual nsresult Init(nsIPresentationControlChannel* aControlChannel);

  const nsAString& GetUrl() const
  {
    return mUrl;
  }

  const nsAString& GetSessionId() const
  {
    return mSessionId;
  }

  uint8_t GetRole() const
  {
    return mRole;
  }

  nsresult SetListener(nsIPresentationSessionListener* aListener);

  void SetDevice(nsIPresentationDevice* aDevice)
  {
    mDevice = aDevice;
  }

  already_AddRefed<nsIPresentationDevice> GetDevice() const
  {
    nsCOMPtr<nsIPresentationDevice> device = mDevice;
    return device.forget();
  }

  void SetControlChannel(nsIPresentationControlChannel* aControlChannel)
  {
    if (mControlChannel) {
      mControlChannel->SetListener(nullptr);
    }

    mControlChannel = aControlChannel;
    if (mControlChannel) {
      mControlChannel->SetListener(this);
    }
  }

  nsresult Send(const nsAString& aData);

  nsresult Close(nsresult aReason,
                 uint32_t aState);

  nsresult ReplyError(nsresult aReason);

  virtual bool IsAccessible(base::ProcessId aProcessId);

protected:
  virtual ~PresentationSessionInfo()
  {
    Shutdown(NS_OK);
  }

  virtual void Shutdown(nsresult aReason);

  nsresult ReplySuccess();

  virtual bool IsSessionReady() = 0;

  virtual nsresult UntrackFromService();

  void SetStateWithReason(uint32_t aState, nsresult aReason)
  {
    if (mState == aState) {
      return;
    }

    mState = aState;
    mReason = aReason;

    // Notify session state change.
    if (mListener) {
      nsresult rv = mListener->NotifyStateChange(mSessionId, mState, aReason);
      NS_WARN_IF(NS_FAILED(rv));
    }
  }

  // Should be nsIPresentationChannelDescription::TYPE_TCP/TYPE_DATACHANNEL
  uint8_t mTransportType = 0;

  nsPIDOMWindowInner* GetWindow();

  nsString mUrl;
  nsString mSessionId;
  // mRole should be nsIPresentationService::ROLE_CONTROLLER
  //              or nsIPresentationService::ROLE_RECEIVER.
  uint8_t mRole;
  bool mIsResponderReady;
  bool mIsTransportReady;
  uint32_t mState; // CONNECTED, CLOSED, TERMINATED
  nsresult mReason;
  nsCOMPtr<nsIPresentationSessionListener> mListener;
  nsCOMPtr<nsIPresentationDevice> mDevice;
  nsCOMPtr<nsIPresentationSessionTransport> mTransport;
  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
  nsCOMPtr<nsIPresentationSessionTransportBuilder> mBuilder;
};

// Session info with controlling browsing context (sender side) behaviors.
class PresentationControllingInfo final : public PresentationSessionInfo
                                        , public nsIServerSocketListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSISERVERSOCKETLISTENER

  PresentationControllingInfo(const nsAString& aUrl,
                              const nsAString& aSessionId)
    : PresentationSessionInfo(aUrl,
                              aSessionId,
                              nsIPresentationService::ROLE_CONTROLLER)
  {}

  nsresult Init(nsIPresentationControlChannel* aControlChannel) override;

private:
  ~PresentationControllingInfo()
  {
    Shutdown(NS_OK);
  }

  void Shutdown(nsresult aReason) override;

  nsresult GetAddress();

  nsresult OnGetAddress(const nsACString& aAddress);

  nsCOMPtr<nsIServerSocket> mServerSocket;

protected:
  bool IsSessionReady() override
  {
    if (mTransportType == nsIPresentationChannelDescription::TYPE_TCP) {
      return mIsResponderReady && mIsTransportReady;
    } else if (mTransportType == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
      // Established RTCDataChannel implies responder is ready.
      return mIsTransportReady;
    }
    return false;
  }
};

// Session info with presenting browsing context (receiver side) behaviors.
class PresentationPresentingInfo final : public PresentationSessionInfo
                                       , public PromiseNativeHandler
                                       , public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSITIMERCALLBACK

  PresentationPresentingInfo(const nsAString& aUrl,
                             const nsAString& aSessionId,
                             nsIPresentationDevice* aDevice)
    : PresentationSessionInfo(aUrl,
                              aSessionId,
                              nsIPresentationService::ROLE_RECEIVER)
  {
    MOZ_ASSERT(aDevice);
    SetDevice(aDevice);
  }

  nsresult Init(nsIPresentationControlChannel* aControlChannel) override;

  nsresult NotifyResponderReady();

  NS_IMETHODIMP OnSessionTransport(nsIPresentationSessionTransport* transport) override;

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void SetPromise(Promise* aPromise)
  {
    mPromise = aPromise;
    mPromise->AppendNativeHandler(this);
  }

  bool IsAccessible(base::ProcessId aProcessId) override;

private:
  ~PresentationPresentingInfo()
  {
    Shutdown(NS_OK);
  }

  void Shutdown(nsresult aReason) override;

  nsresult InitTransportAndSendAnswer();

  nsresult UntrackFromService() override;

  RefPtr<PresentationResponderLoadingCallback> mLoadingCallback;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIPresentationChannelDescription> mRequesterDescription;
  RefPtr<Promise> mPromise;

  // The content parent communicating with the content process which the OOP
  // receiver page belongs to.
  nsCOMPtr<nsIContentParent> mContentParent;

protected:
  bool IsSessionReady() override
  {
    return mIsResponderReady && mIsTransportReady;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationSessionInfo_h
