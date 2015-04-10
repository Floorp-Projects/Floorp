/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputPort_h
#define mozilla_dom_InputPort_h

#include "InputPortListeners.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/InputPortBinding.h"
#include "nsIInputPortService.h"

namespace mozilla {

class DOMMediaStream;

namespace dom {

class InputPort : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InputPort, DOMEventTargetHelper)

  // WebIDL (internal functions)
  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  void NotifyConnectionChanged(bool aIsConnected);

  // WebIDL (public APIs)
  void GetId(nsAString& aId) const;

  DOMMediaStream* Stream() const;

  bool Connected() const;

  IMPL_EVENT_HANDLER(connect);
  IMPL_EVENT_HANDLER(disconnect);

protected:
  explicit InputPort(nsPIDOMWindow* aWindow);

  virtual ~InputPort();

  void Init(nsIInputPortData* aData, nsIInputPortListener* aListener, ErrorResult& aRv);
  void Shutdown();

  nsString mId;
  nsRefPtr<DOMMediaStream> mStream;
  nsRefPtr<InputPortListener> mInputPortListener;
  bool mIsConnected;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputPort_h
