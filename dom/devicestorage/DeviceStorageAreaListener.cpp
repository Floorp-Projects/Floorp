/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DeviceStorageAreaListener.h"
#include "mozilla/dom/DeviceStorageAreaListenerBinding.h"
#include "mozilla/Attributes.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#endif

namespace mozilla {
namespace dom {

class VolumeStateObserver final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit VolumeStateObserver(DeviceStorageAreaListener* aListener)
    : mDeviceStorageAreaListener(aListener) {}
  void ForgetListener() { mDeviceStorageAreaListener = nullptr; }

private:
  ~VolumeStateObserver() {};

  // This reference is non-owning and it's cleared by
  // DeviceStorageAreaListener's destructor.
  DeviceStorageAreaListener* MOZ_NON_OWNING_REF mDeviceStorageAreaListener;
};

NS_IMPL_ISUPPORTS(VolumeStateObserver, nsIObserver)

NS_IMETHODIMP
VolumeStateObserver::Observe(nsISupports *aSubject,
                             const char *aTopic,
                             const char16_t *aData)
{
  if (!mDeviceStorageAreaListener) {
    return NS_OK;
  }

#ifdef MOZ_WIDGET_GONK
  if (!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
    nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
    MOZ_ASSERT(vol);

    int32_t state;
    nsresult rv = vol->GetState(&state);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString volName;
    vol->GetName(volName);

    switch (state) {
      case nsIVolume::STATE_MOUNTED:
        mDeviceStorageAreaListener->DispatchStorageAreaChangedEvent(
          volName,
          DeviceStorageAreaChangedEventOperation::Added);
        break;
      default:
        mDeviceStorageAreaListener->DispatchStorageAreaChangedEvent(
          volName,
          DeviceStorageAreaChangedEventOperation::Removed);
        break;
    }
  }
#endif
  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(DeviceStorageAreaListener, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DeviceStorageAreaListener, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN(DeviceStorageAreaListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

DeviceStorageAreaListener::DeviceStorageAreaListener(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_ASSERT(aWindow);

  mVolumeStateObserver = new VolumeStateObserver(this);
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(mVolumeStateObserver, NS_VOLUME_STATE_CHANGED, false);
  }
#endif
}

DeviceStorageAreaListener::~DeviceStorageAreaListener()
{
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(mVolumeStateObserver, NS_VOLUME_STATE_CHANGED);
  }
#endif
  mVolumeStateObserver->ForgetListener();
}

JSObject*
DeviceStorageAreaListener::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DeviceStorageAreaListenerBinding::Wrap(aCx, this, aGivenProto);
}

void
DeviceStorageAreaListener::DispatchStorageAreaChangedEvent(
  const nsString& aStorageName,
  DeviceStorageAreaChangedEventOperation aOperation)
{
  StateMapType::const_iterator iter = mStorageAreaStateMap.find(aStorageName);
  if (iter == mStorageAreaStateMap.end() &&
      aOperation != DeviceStorageAreaChangedEventOperation::Added) {
    // The operation of the first event to dispatch should be "Added".
    return;
  }
  if (iter != mStorageAreaStateMap.end() &&
      iter->second == aOperation) {
    // No need to disptach the event if the state is unchanged.
    return;
  }

  DeviceStorageAreaChangedEventInit init;
  init.mOperation = aOperation;
  init.mStorageName = aStorageName;

  nsRefPtr<DeviceStorageAreaChangedEvent> event =
    DeviceStorageAreaChangedEvent::Constructor(this,
                                               NS_LITERAL_STRING("storageareachanged"),
                                               init);
  event->SetTrusted(true);

  bool ignore;
  DOMEventTargetHelper::DispatchEvent(event, &ignore);

  mStorageAreaStateMap[aStorageName] = aOperation;
}

} // namespace dom
} // namespace mozilla
