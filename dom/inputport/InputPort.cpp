/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraph.h"
#include "DOMMediaStream.h"
#include "InputPortData.h"
#include "InputPortListeners.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/InputPort.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(InputPort, DOMEventTargetHelper,
                                   mStream,
                                   mInputPortListener)

NS_IMPL_ADDREF_INHERITED(InputPort, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(InputPort, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InputPort)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

InputPort::InputPort(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mIsConnected(false)
{
}

InputPort::~InputPort()
{
}

void
InputPort::Init(nsIInputPortData* aData, nsIInputPortListener* aListener, ErrorResult& aRv)
{
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aListener);

  aRv = aData->GetId(mId);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (NS_WARN_IF(mId.IsEmpty())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  InputPortType type = static_cast<InputPortData*>(aData)->GetType();
  if (NS_WARN_IF(type == InputPortType::EndGuard_)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aData->GetConnected(&mIsConnected);

  mInputPortListener = static_cast<InputPortListener*>(aListener);
  mInputPortListener->RegisterInputPort(this);

  MediaStreamGraph* graph =
    MediaStreamGraph::GetInstance(MediaStreamGraph::SYSTEM_THREAD_DRIVER,
                                  AudioChannel::Normal);
  mStream = DOMMediaStream::CreateSourceStreamAsInput(GetOwner(), graph);
}

void
InputPort::Shutdown()
{
  MOZ_ASSERT(mInputPortListener);
  if (mInputPortListener) {
    mInputPortListener->UnregisterInputPort(this);
    mInputPortListener = nullptr;
  }
}

/* virtual */ JSObject*
InputPort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return InputPortBinding::Wrap(aCx, this, aGivenProto);
}

void
InputPort::NotifyConnectionChanged(bool aIsConnected)
{
  MOZ_ASSERT(mIsConnected != aIsConnected);
  mIsConnected = aIsConnected;

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this,
                             aIsConnected ? NS_LITERAL_STRING("connect") :
                                            NS_LITERAL_STRING("disconnect"),
                             false);
  asyncDispatcher->PostDOMEvent();
}

void
InputPort::GetId(nsAString& aId) const
{
  aId = mId;
}

DOMMediaStream*
InputPort::Stream() const
{
  return mStream;
}

bool
InputPort::Connected() const
{
  return mIsConnected;
}

} // namespace dom
} // namespace mozilla
