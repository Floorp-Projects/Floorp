/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DisplayPortInputPort.h"
#include "mozilla/dom/DisplayPortInputPortBinding.h"

namespace mozilla {
namespace dom {

DisplayPortInputPort::DisplayPortInputPort(nsPIDOMWindow* aWindow)
  : InputPort(aWindow)
{
}

DisplayPortInputPort::~DisplayPortInputPort()
{
}

/* static */ already_AddRefed<DisplayPortInputPort>
DisplayPortInputPort::Create(nsPIDOMWindow* aWindow,
                             nsIInputPortListener* aListener,
                             nsIInputPortData* aData,
                             ErrorResult& aRv)
{
  nsRefPtr<DisplayPortInputPort> inputport = new DisplayPortInputPort(aWindow);
  inputport->Init(aData, aListener, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return inputport.forget();
}

JSObject*
DisplayPortInputPort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DisplayPortInputPortBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} //namespace mozilla
