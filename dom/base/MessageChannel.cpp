/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessageChannel.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/MessageChannelBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(MessageChannel, mWindow, mPort1, mPort2)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MessageChannel)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MessageChannel)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessageChannel)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

namespace {
  bool gPrefInitialized = false;
  bool gPrefEnabled = false;

}


/* static */ bool
MessageChannel::PrefEnabled()
{
  if (!gPrefInitialized) {
    Preferences::AddBoolVarCache(&gPrefEnabled, "dom.messageChannel.enabled");
    gPrefInitialized = true;
  }

  return gPrefEnabled;
}

MessageChannel::MessageChannel(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  MOZ_COUNT_CTOR(MessageChannel);
  SetIsDOMBinding();

  mPort1 = new MessagePort(mWindow);
  mPort2 = new MessagePort(mWindow);

  mPort1->Entangle(mPort2);
  mPort2->Entangle(mPort1);
}

MessageChannel::~MessageChannel()
{
  MOZ_COUNT_DTOR(MessageChannel);
}

JSObject*
MessageChannel::WrapObject(JSContext* aCx)
{
  return MessageChannelBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<MessageChannel>
MessageChannel::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<MessageChannel> channel = new MessageChannel(window);
  return channel.forget();
}

} // namespace dom
} // namespace mozilla
