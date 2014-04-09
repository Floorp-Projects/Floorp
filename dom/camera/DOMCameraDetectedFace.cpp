/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraDetectedFace.h"
#include "Navigator.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(DOMCameraPoint, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMCameraPoint)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMCameraPoint)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMCameraPoint)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_5(DOMCameraDetectedFace, mParent,
                                        mBounds, mLeftEye, mRightEye, mMouth)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMCameraDetectedFace)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMCameraDetectedFace)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMCameraDetectedFace)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
bool
DOMCameraPoint::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

/* static */
bool
DOMCameraDetectedFace::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

JSObject*
DOMCameraPoint::WrapObject(JSContext* aCx)
{
  return CameraPointBinding::Wrap(aCx, this);
}

JSObject*
DOMCameraDetectedFace::WrapObject(JSContext* aCx)
{
  return CameraDetectedFaceBinding::Wrap(aCx, this);
}

DOMCameraDetectedFace::DOMCameraDetectedFace(nsISupports* aParent,
                                             const ICameraControl::Face& aFace)
  : mParent(aParent)
  , mId(aFace.id)
  , mScore(aFace.score)
  , mBounds(new DOMRect(MOZ_THIS_IN_INITIALIZER_LIST()))
{
  mBounds->SetRect(aFace.bound.left,
                   aFace.bound.top,
                   aFace.bound.right - aFace.bound.left,
                   aFace.bound.bottom - aFace.bound.top);

  if (aFace.hasLeftEye) {
    mLeftEye = new DOMCameraPoint(this, aFace.leftEye);
  }
  if (aFace.hasRightEye) {
    mRightEye = new DOMCameraPoint(this, aFace.rightEye);
  }
  if (aFace.hasMouth) {
    mMouth = new DOMCameraPoint(this, aFace.mouth);
  }

  SetIsDOMBinding();
}
