/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AVInputPort.h"
#include "mozilla/dom/AVInputPortBinding.h"

namespace mozilla {
namespace dom {

AVInputPort::AVInputPort(nsPIDOMWindowInner* aWindow)
  : InputPort(aWindow)
{
}

AVInputPort::~AVInputPort()
{
}

/* static */ already_AddRefed<AVInputPort>
AVInputPort::Create(nsPIDOMWindowInner* aWindow,
                    nsIInputPortListener* aListener,
                    nsIInputPortData* aData,
                    ErrorResult& aRv)
{
  RefPtr<AVInputPort> inputport = new AVInputPort(aWindow);
  inputport->Init(aData, aListener, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return inputport.forget();
}

JSObject*
AVInputPort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AVInputPortBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} //namespace mozilla
