/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenOrientation_h
#define mozilla_dom_ScreenOrientation_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ScreenOrientationBinding.h"
#include "mozilla/HalScreenConfiguration.h"

class nsScreen;

namespace mozilla {
namespace dom {

class Promise;
// Make sure that any change to ScreenOrientationInternal values are
// also made in mobile/android/base/GeckoScreenOrientation.java
typedef uint32_t ScreenOrientationInternal;

static const ScreenOrientationInternal eScreenOrientation_None               = 0;
static const ScreenOrientationInternal eScreenOrientation_PortraitPrimary    = 1u << 0;
static const ScreenOrientationInternal eScreenOrientation_PortraitSecondary  = 1u << 1;
static const ScreenOrientationInternal eScreenOrientation_LandscapePrimary   = 1u << 2;
static const ScreenOrientationInternal eScreenOrientation_LandscapeSecondary = 1u << 3;
//eScreenOrientation_Default will use the natural orientation for the deivce,
//it could be PortraitPrimary or LandscapePrimary depends on display resolution
static const ScreenOrientationInternal eScreenOrientation_Default            = 1u << 4;

class ScreenOrientation final : public DOMEventTargetHelper,
                                public mozilla::hal::ScreenConfigurationObserver
{
  // nsScreen has deprecated API that shares implementation.
  friend class ::nsScreen;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ScreenOrientation, mozilla::DOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(change)

  ScreenOrientation(nsPIDOMWindowInner* aWindow, nsScreen* aScreen);

  already_AddRefed<Promise> Lock(OrientationLockType aOrientation,
                                 ErrorResult& aRv);

  void Unlock(ErrorResult& aRv);

  // DeviceType and DeviceAngle gets the current type and angle of the device.
  OrientationType DeviceType() const;
  uint16_t DeviceAngle() const;

  // GetType and GetAngle gets the type and angle of the responsible document
  // (as defined in specification).
  OrientationType GetType(ErrorResult& aRv) const;
  uint16_t GetAngle(ErrorResult& aRv) const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration) override;

  static void UpdateActiveOrientationLock(ScreenOrientationInternal aOrientation);

private:
  virtual ~ScreenOrientation();

  // Listener to unlock orientation if we leave fullscreen.
  class FullScreenEventListener;

  // Listener to update document's orienation lock when document becomes
  // visible.
  class VisibleEventListener;

  // Task to run step to lock orientation as defined in specification.
  class LockOrientationTask;

  enum LockPermission {
    LOCK_DENIED,
    FULLSCREEN_LOCK_ALLOWED,
    LOCK_ALLOWED
  };

  // This method calls into the HAL to lock the device and sets
  // up listeners for full screen change.
  bool LockDeviceOrientation(ScreenOrientationInternal aOrientation,
                             bool aIsFullscreen, ErrorResult& aRv);

  // This method calls in to the HAL to unlock the device and removes
  // full screen change listener.
  void UnlockDeviceOrientation();

  // This method performs the same function as |Lock| except it takes
  // a ScreenOrientationInternal argument instead of an OrientationType.
  // This method exists in order to share implementation with nsScreen that
  // uses ScreenOrientationInternal.
  already_AddRefed<Promise> LockInternal(ScreenOrientationInternal aOrientation,
                                         ErrorResult& aRv);

  void DispatchChangeEvent();

  bool ShouldResistFingerprinting() const;

  LockPermission GetLockOrientationPermission(bool aCheckSandbox) const;

  // Gets the responsible document as defined in the spec.
  nsIDocument* GetResponsibleDocument() const;

  RefPtr<nsScreen> mScreen;
  RefPtr<FullScreenEventListener> mFullScreenListener;
  RefPtr<VisibleEventListener> mVisibleListener;
  OrientationType mType;
  uint16_t mAngle;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScreenOrientation_h
