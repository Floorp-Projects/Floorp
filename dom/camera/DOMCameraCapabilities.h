/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERACAPABILITIES_H
#define DOM_CAMERA_DOMCAMERACAPABILITIES_H

#include "ICameraControl.h"
#include "nsAutoPtr.h"
#include "CameraCommon.h"

namespace mozilla {

typedef nsresult (*ParseItemAndAddFunc)(JSContext* aCx, JS::Handle<JSObject*> aArray,
                                        uint32_t aIndex, const char* aStart, char** aEnd);

class DOMCameraCapabilities MOZ_FINAL : public nsICameraCapabilities
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICAMERACAPABILITIES

  DOMCameraCapabilities(ICameraControl* aCamera)
    : mCamera(aCamera)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  nsresult ParameterListToNewArray(
    JSContext* cx,
    JS::MutableHandle<JSObject*> aArray,
    uint32_t aKey,
    ParseItemAndAddFunc aParseItemAndAdd
  );
  nsresult StringListToNewObject(JSContext* aCx, JS::Value* aArray, uint32_t aKey);
  nsresult DimensionListToNewObject(JSContext* aCx, JS::Value* aArray, uint32_t aKey);

private:
  DOMCameraCapabilities(const DOMCameraCapabilities&) MOZ_DELETE;
  DOMCameraCapabilities& operator=(const DOMCameraCapabilities&) MOZ_DELETE;

protected:
  /* additional members */
  ~DOMCameraCapabilities()
  {
    // destructor code
    DOM_CAMERA_LOGT("%s:%d : this=%p, mCamera=%p\n", __func__, __LINE__, this, mCamera.get());
  }

  nsRefPtr<ICameraControl> mCamera;
};

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERACAPABILITIES_H
