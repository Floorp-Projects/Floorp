/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraDetectedFace.h"
#include "Navigator.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMCameraDetectedFace, mParent,
                                      mBounds, mLeftEye, mRightEye, mMouth)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMCameraDetectedFace)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMCameraDetectedFace)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMCameraDetectedFace)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
bool
DOMCameraDetectedFace::HasSupport(JSContext* aCx, JSObject* aGlobal)
{
  return Navigator::HasCameraSupport(aCx, aGlobal);
}

JSObject*
DOMCameraDetectedFace::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CameraDetectedFaceBinding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<DOMCameraDetectedFace>
DOMCameraDetectedFace::Constructor(const GlobalObject& aGlobal,
                                   const dom::CameraDetectedFaceInit& aFace,
                                   ErrorResult& aRv)
{
  RefPtr<DOMCameraDetectedFace> face =
    new DOMCameraDetectedFace(aGlobal.GetAsSupports(), aFace);
  return face.forget();
}

DOMCameraDetectedFace::DOMCameraDetectedFace(nsISupports* aParent,
                                             const dom::CameraDetectedFaceInit& aFace)
  : mParent(aParent)
  , mId(aFace.mId)
  , mScore(aFace.mScore)
  , mBounds(new DOMRect(this))
{
  mBounds->SetRect(aFace.mBounds.mLeft,
                   aFace.mBounds.mTop,
                   aFace.mBounds.mRight - aFace.mBounds.mLeft,
                   aFace.mBounds.mBottom - aFace.mBounds.mTop);

  if (aFace.mHasLeftEye) {
    mLeftEye = new DOMPoint(this, aFace.mLeftEye.mX, aFace.mLeftEye.mY);
  }
  if (aFace.mHasRightEye) {
    mRightEye = new DOMPoint(this, aFace.mRightEye.mX, aFace.mRightEye.mY);
  }
  if (aFace.mHasMouth) {
    mMouth = new DOMPoint(this, aFace.mMouth.mX, aFace.mMouth.mY);
  }
}

DOMCameraDetectedFace::DOMCameraDetectedFace(nsISupports* aParent,
                                             const ICameraControl::Face& aFace)
  : mParent(aParent)
  , mId(aFace.id)
  , mScore(aFace.score)
  , mBounds(new DOMRect(this))
{
  mBounds->SetRect(aFace.bound.left,
                   aFace.bound.top,
                   aFace.bound.right - aFace.bound.left,
                   aFace.bound.bottom - aFace.bound.top);

  if (aFace.hasLeftEye) {
    mLeftEye = new DOMPoint(this, aFace.leftEye.x, aFace.leftEye.y);
  }
  if (aFace.hasRightEye) {
    mRightEye = new DOMPoint(this, aFace.rightEye.x, aFace.rightEye.y);
  }
  if (aFace.hasMouth) {
    mMouth = new DOMPoint(this, aFace.mouth.x, aFace.mouth.y);
  }
}
