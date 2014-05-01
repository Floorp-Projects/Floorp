/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERADETECTEDFACE_H
#define DOM_CAMERA_DOMCAMERADETECTEDFACE_H

#include "mozilla/dom/CameraControlBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/DOMPoint.h"
#include "ICameraControl.h"

namespace mozilla {

namespace dom {

class DOMCameraDetectedFace MOZ_FINAL : public nsISupports
                                      , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMCameraDetectedFace)

  // Because this header's filename doesn't match its C++ or DOM-facing
  // classname, we can't rely on the [Func="..."] WebIDL tag to implicitly
  // include the right header for us; instead we must explicitly include a
  // HasSupport() method in each header. We can get rid of these with the
  // Great Renaming proposed in bug 983177.
  static bool HasSupport(JSContext* aCx, JSObject* aGlobal);

  DOMCameraDetectedFace(nsISupports* aParent, const ICameraControl::Face& aFace);

  uint32_t Id()       { return mId; }
  uint32_t Score()    { return mScore; }
  bool HasLeftEye()   { return mLeftEye; }
  bool HasRightEye()  { return mRightEye; }
  bool HasMouth()     { return mMouth; }

  dom::DOMRect* Bounds()        { return mBounds; }

  dom::DOMPoint* GetLeftEye()  { return mLeftEye; }
  dom::DOMPoint* GetRightEye() { return mRightEye; }
  dom::DOMPoint* GetMouth()    { return mMouth; }

  nsISupports*
  GetParentObject() const
  {
    MOZ_ASSERT(mParent);
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

protected:
  virtual ~DOMCameraDetectedFace() { }

  nsCOMPtr<nsISupports> mParent;

  uint32_t mId;
  uint32_t mScore;

  nsRefPtr<dom::DOMRect> mBounds;

  nsRefPtr<dom::DOMPoint> mLeftEye;
  nsRefPtr<dom::DOMPoint> mRightEye;
  nsRefPtr<dom::DOMPoint> mMouth;
};

} // namespace dom

} // namespace mozilla

#endif // DOM_CAMERA_DOMCAMERADETECTEDFACE_H
