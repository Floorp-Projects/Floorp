/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HDMIInputPort.h"
#include "mozilla/dom/HDMIInputPortBinding.h"

namespace mozilla {
namespace dom {

HDMIInputPort::HDMIInputPort(nsPIDOMWindowInner* aWindow)
  : InputPort(aWindow)
{
}

HDMIInputPort::~HDMIInputPort()
{
}

/* static */ already_AddRefed<HDMIInputPort>
HDMIInputPort::Create(nsPIDOMWindowInner* aWindow,
                      nsIInputPortListener* aListener,
                      nsIInputPortData* aData,
                      ErrorResult& aRv)
{
  RefPtr<HDMIInputPort> inputport = new HDMIInputPort(aWindow);
  inputport->Init(aData, aListener, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return inputport.forget();
}


JSObject*
HDMIInputPort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HDMIInputPortBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} //namespace mozilla
