/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaDeviceInfo_h
#define mozilla_dom_MediaDeviceInfo_h

#include "mozilla/ErrorResult.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/BindingUtils.h"
#include "MediaDeviceInfoBinding.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

#define MOZILLA_DOM_MEDIADEVICEINFO_IMPLEMENTATION_IID \
{0x25091870, 0x84d6, 0x4acf, {0xaf, 0x97, 0x6e, 0xd5, 0x5b, 0xe0, 0x47, 0xb2}}

class MediaDeviceInfo final : public nsISupports, public nsWrapperCache
{
public:
  explicit MediaDeviceInfo(const nsAString& aDeviceId,
                           MediaDeviceKind aKind,
                           const nsAString& aLabel,
                           const nsAString& aGroupId = nsString());

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaDeviceInfo)
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIADEVICEINFO_IMPLEMENTATION_IID)

  JSObject*
  WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject();

  void GetDeviceId(nsString& retval);
  MediaDeviceKind Kind();
  void GetLabel(nsString& retval);
  void GetGroupId(nsString& retval);

private:
  MediaDeviceKind mKind;
  nsString mDeviceId;
  nsString mLabel;
  nsString mGroupId;

  virtual ~MediaDeviceInfo() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaDeviceInfo,
                              MOZILLA_DOM_MEDIADEVICEINFO_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaDeviceInfo_h
