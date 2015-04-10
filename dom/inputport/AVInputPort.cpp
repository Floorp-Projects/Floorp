/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AVInputPort.h"
#include "mozilla/dom/AVInputPortBinding.h"

namespace mozilla {
namespace dom {

AVInputPort::AVInputPort(nsPIDOMWindow* aWindow)
  : InputPort(aWindow)
{
}

AVInputPort::~AVInputPort()
{
}

/* static */ already_AddRefed<AVInputPort>
AVInputPort::Create(nsPIDOMWindow* aWindow,
                    nsIInputPortListener* aListener,
                    nsIInputPortData* aData,
                    ErrorResult& aRv)
{
  nsRefPtr<AVInputPort> inputport = new AVInputPort(aWindow);
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
