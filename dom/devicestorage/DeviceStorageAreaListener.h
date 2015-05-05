/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DeviceStorageAreaListener_h
#define mozilla_dom_DeviceStorageAreaListener_h

#include <map>
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DeviceStorageAreaChangedEvent.h"

namespace mozilla {
namespace dom {

class VolumeStateObserver;

class DeviceStorageAreaListener final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  IMPL_EVENT_HANDLER(storageareachanged)

  explicit DeviceStorageAreaListener(nsPIDOMWindow* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  friend class VolumeStateObserver;

  typedef std::map<nsString, DeviceStorageAreaChangedEventOperation> StateMapType;
  StateMapType mStorageAreaStateMap;

  nsRefPtr<VolumeStateObserver> mVolumeStateObserver;

  ~DeviceStorageAreaListener();

  void DispatchStorageAreaChangedEvent(
    const nsString& aStorageName,
    DeviceStorageAreaChangedEventOperation aOperation);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DeviceStorageAreaListener_h
