/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_NSCAMERACAPABILITIES_H
#define DOM_CAMERA_NSCAMERACAPABILITIES_H

#include "CameraControl.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

namespace mozilla {

typedef nsresult (*ParseItemAndAddFunc)(JSContext* aCx, JSObject* aArray, PRUint32 aIndex, const char* aStart, char** aEnd);

class nsCameraCapabilities MOZ_FINAL : public nsICameraCapabilities
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICAMERACAPABILITIES

  nsCameraCapabilities(nsCameraControl* aCamera);

  nsresult ParameterListToNewArray(
    JSContext* cx,
    JSObject** aArray,
    PRUint32 aKey,
    ParseItemAndAddFunc aParseItemAndAdd
  );
  nsresult StringListToNewObject(JSContext* aCx, JS::Value* aArray, PRUint32 aKey);
  nsresult DimensionListToNewObject(JSContext* aCx, JS::Value* aArray, PRUint32 aKey);

private:
  nsCameraCapabilities(const nsCameraCapabilities&) MOZ_DELETE;
  nsCameraCapabilities& operator=(const nsCameraCapabilities&) MOZ_DELETE;

protected:
  /* additional members */
  ~nsCameraCapabilities();
  nsCOMPtr<nsCameraControl> mCamera;
};

} // namespace mozilla

#endif // DOM_CAMERA_NSCAMERACAPABILITIES_H
