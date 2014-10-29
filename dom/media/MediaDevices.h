/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDevices_h__
#define MediaDevices_h__

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

class Promise;
struct MediaStreamConstraints;

#define MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID \
{ 0x2f784d8a, 0x7485, 0x4280, \
 { 0x9a, 0x36, 0x74, 0xa4, 0xd6, 0x71, 0xa6, 0xc8 } }

class MediaDevices MOZ_FINAL : public DOMEventTargetHelper
{
public:
  MediaDevices(nsPIDOMWindow* aWindow) : DOMEventTargetHelper(aWindow) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID)

  JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

  already_AddRefed<Promise>
  GetUserMedia(const MediaStreamConstraints& aConstraints, ErrorResult &aRv);

private:
  class GumResolver;
  class GumRejecter;

  virtual ~MediaDevices() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaDevices,
                              MOZILLA_DOM_MEDIADEVICES_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // MediaDevices_h__
