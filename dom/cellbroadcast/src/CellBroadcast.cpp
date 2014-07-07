/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcast.h"
#include "mozilla/dom/MozCellBroadcastBinding.h"
#include "mozilla/dom/MozCellBroadcastEvent.h"
#include "nsIDOMMozCellBroadcastMessage.h"
#include "nsServiceManagerUtils.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

using namespace mozilla::dom;

/**
 * CellBroadcast::Listener Implementation.
 */

class CellBroadcast::Listener MOZ_FINAL : public nsICellBroadcastListener
{
private:
  CellBroadcast* mCellBroadcast;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSICELLBROADCASTLISTENER(mCellBroadcast)

  Listener(CellBroadcast* aCellBroadcast)
    : mCellBroadcast(aCellBroadcast)
  {
    MOZ_ASSERT(mCellBroadcast);
  }

  void Disconnect()
  {
    MOZ_ASSERT(mCellBroadcast);
    mCellBroadcast = nullptr;
  }
};

NS_IMPL_ISUPPORTS(CellBroadcast::Listener, nsICellBroadcastListener)

/**
 * CellBroadcast Implementation.
 */

// static
already_AddRefed<CellBroadcast>
CellBroadcast::Create(nsPIDOMWindow* aWindow, ErrorResult& aRv)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  nsCOMPtr<nsICellBroadcastProvider> provider =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  if (!provider) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<CellBroadcast> cb = new CellBroadcast(aWindow, provider);
  return cb.forget();
}

CellBroadcast::CellBroadcast(nsPIDOMWindow *aWindow,
                             nsICellBroadcastProvider *aProvider)
  : DOMEventTargetHelper(aWindow)
  , mProvider(aProvider)
{
  mListener = new Listener(this);
  DebugOnly<nsresult> rv = mProvider->RegisterCellBroadcastMsg(mListener);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering Cell Broadcast callback with provider");
}

CellBroadcast::~CellBroadcast()
{
  MOZ_ASSERT(mProvider && mListener);

  mListener->Disconnect();
  mProvider->UnregisterCellBroadcastMsg(mListener);
}

JSObject*
CellBroadcast::WrapObject(JSContext* aCx)
{
  return MozCellBroadcastBinding::Wrap(aCx, this);
}

// Forwarded nsICellBroadcastListener methods

NS_IMETHODIMP
CellBroadcast::NotifyMessageReceived(nsIDOMMozCellBroadcastMessage* aMessage)
{
  MozCellBroadcastEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mMessage = aMessage;

  nsRefPtr<MozCellBroadcastEvent> event =
    MozCellBroadcastEvent::Constructor(this, NS_LITERAL_STRING("received"), init);
  return DispatchTrustedEvent(event);
}
