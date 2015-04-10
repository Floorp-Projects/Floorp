/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AVInputPort_h
#define mozilla_dom_AVInputPort_h

#include "mozilla/dom/InputPort.h"

namespace mozilla {
namespace dom {

class AVInputPort final : public InputPort
{
public:
  static already_AddRefed<AVInputPort> Create(nsPIDOMWindow* aWindow,
                                              nsIInputPortListener* aListener,
                                              nsIInputPortData* aData,
                                              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  explicit AVInputPort(nsPIDOMWindow* aWindow);

  ~AVInputPort();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AVInputPort_h
