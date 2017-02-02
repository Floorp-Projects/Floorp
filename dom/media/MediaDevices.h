/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaDevices_h
#define mozilla_dom_MediaDevices_h

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/media/DeviceChangeCallback.h"

namespace mozilla {
namespace dom {

class Promise;
struct MediaStreamConstraints;
struct MediaTrackSupportedConstraints;

#define MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID \
{ 0x2f784d8a, 0x7485, 0x4280, \
 { 0x9a, 0x36, 0x74, 0xa4, 0xd6, 0x71, 0xa6, 0xc8 } }

class MediaDevices final : public DOMEventTargetHelper
                          ,public DeviceChangeCallback
{
public:
  explicit MediaDevices(nsPIDOMWindowInner* aWindow) :
    DOMEventTargetHelper(aWindow) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID)

  JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  // No code needed, as MediaTrackSupportedConstraints members default to true.
  void GetSupportedConstraints(MediaTrackSupportedConstraints& aResult) {};

  already_AddRefed<Promise>
  GetUserMedia(const MediaStreamConstraints& aConstraints,
	       CallerType aCallerType, ErrorResult &aRv);

  already_AddRefed<Promise>
  EnumerateDevices(ErrorResult &aRv);

  virtual void OnDeviceChange() override;

  mozilla::dom::EventHandlerNonNull* GetOndevicechange();

  void SetOndevicechange(mozilla::dom::EventHandlerNonNull* aCallback);

  NS_IMETHOD AddEventListener(const nsAString& aType,
    nsIDOMEventListener* aListener,
    bool aUseCapture, bool aWantsUntrusted,
    uint8_t optional_argc) override;

  virtual void AddEventListener(const nsAString& aType,
                                dom::EventListener* aListener,
                                const dom::AddEventListenerOptionsOrBoolean& aOptions,
                                const dom::Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) override;

private:
  class GumResolver;
  class EnumDevResolver;
  class GumRejecter;

  virtual ~MediaDevices();
  nsCOMPtr<nsITimer> mFuzzTimer;
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaDevices,
                              MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaDevices_h
