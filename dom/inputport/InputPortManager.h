/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputPortManager_h
#define mozilla_dom_InputPortManager_h

#include "nsIInputPortService.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIInputPortService;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;
class InputPort;

class InputPortManager final : public nsIInputPortServiceCallback,
                               public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(InputPortManager)
  NS_DECL_NSIINPUTPORTSERVICECALLBACK

  static already_AddRefed<InputPortManager> Create(nsPIDOMWindow* aWindow, ErrorResult& aRv);

  // WebIDL (internal functions)
  nsPIDOMWindow* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  already_AddRefed<Promise> GetInputPorts(ErrorResult& aRv);

private:
  explicit InputPortManager(nsPIDOMWindow* aWindow);

  ~InputPortManager();

  void Init(ErrorResult& aRv);

  void RejectPendingGetInputPortsPromises(nsresult aRv);

  nsresult SetInputPorts(const nsTArray<nsRefPtr<InputPort>>& aPorts);

  nsTArray<nsRefPtr<Promise>> mPendingGetInputPortsPromises;
  nsTArray<nsRefPtr<InputPort>> mInputPorts;
  nsCOMPtr<nsPIDOMWindow> mParent;
  nsCOMPtr<nsIInputPortService> mInputPortService;
  bool mIsReady;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputPortManager_h__
