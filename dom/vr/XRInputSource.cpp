/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRInputSource.h"
#include "mozilla/dom/XRInputSourceEvent.h"
#include "XRNativeOriginViewer.h"
#include "XRNativeOriginTracker.h"

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRInputSource, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRInputSource, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRInputSource, Release)

// Follow the controller profile ids from
// https://github.com/immersive-web/webxr-input-profiles.
nsTArray<nsString> GetInputSourceProfile(VRControllerType aType) {
  nsTArray<nsString> profile;
  nsString id;

  switch (aType) {
    case VRControllerType::HTCVive:
      id.AssignLiteral("htc-vive");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-touchpad");
      profile.AppendElement(id);
      break;
    case VRControllerType::HTCViveCosmos:
      id.AssignLiteral("htc-vive-cosmos");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-thumbstick");
      profile.AppendElement(id);
      break;
    case VRControllerType::HTCViveFocus:
      id.AssignLiteral("htc-vive-focus");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-touchpad");
      profile.AppendElement(id);
      break;
    case VRControllerType::HTCViveFocusPlus:
      id.AssignLiteral("htc-vive-focus-plus");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-touchpad");
      profile.AppendElement(id);
      break;
    case VRControllerType::MSMR:
      id.AssignLiteral("microsoft-mixed-reality");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-touchpad-thumbstick");
      profile.AppendElement(id);
      break;
    case VRControllerType::ValveIndex:
      id.AssignLiteral("valve-index");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-touchpad-thumbstick");
      profile.AppendElement(id);
      break;
    case VRControllerType::OculusGo:
      id.AssignLiteral("oculus-go");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-touchpad");
      profile.AppendElement(id);
      break;
    case VRControllerType::OculusTouch:
      id.AssignLiteral("oculus-touch");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-thumbstick");
      profile.AppendElement(id);
      break;
    case VRControllerType::OculusTouch2:
      id.AssignLiteral("oculus-touch-v2");
      profile.AppendElement(id);
      id.AssignLiteral("oculus-touch");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-thumbstick");
      profile.AppendElement(id);
      break;
    case VRControllerType::PicoGaze:
      id.AssignLiteral("pico-gaze");
      profile.AppendElement(id);
      id.AssignLiteral("generic-button");
      profile.AppendElement(id);
      break;
    case VRControllerType::PicoG2:
      id.AssignLiteral("pico-g-v2");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-touchpad");
      profile.AppendElement(id);
      break;
    case VRControllerType::PicoNeo2:
      id.AssignLiteral("pico-neco-v2");
      profile.AppendElement(id);
      id.AssignLiteral("generic-trigger-squeeze-thumbstick");
      profile.AppendElement(id);
      break;
    default:
      NS_WARNING("Unsupported XR input source profile.\n");
      break;
  }
  return profile;
}

XRInputSource::XRInputSource(nsISupports* aParent)
    : mParent(aParent),
      mGamepad(nullptr),
      mIndex(-1),
      mSelectAction(ActionState::ActionState_Released),
      mSqueezeAction(ActionState::ActionState_Released) {}

XRInputSource::~XRInputSource() {
  mTargetRaySpace = nullptr;
  mGripSpace = nullptr;
  mGamepad = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject* XRInputSource::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return XRInputSource_Binding::Wrap(aCx, this, aGivenProto);
}

XRHandedness XRInputSource::Handedness() { return mHandedness; }

XRTargetRayMode XRInputSource::TargetRayMode() { return mTargetRayMode; }

XRSpace* XRInputSource::TargetRaySpace() { return mTargetRaySpace; }

XRSpace* XRInputSource::GetGripSpace() { return mGripSpace; }

void XRInputSource::GetProfiles(nsTArray<nsString>& aResult) {
  aResult = mProfiles;
}

Gamepad* XRInputSource::GetGamepad() { return mGamepad; }

void XRInputSource::Setup(XRSession* aSession, uint32_t aIndex) {
  MOZ_ASSERT(aSession);
  gfx::VRDisplayClient* displayClient = aSession->GetDisplayClient();
  if (!displayClient) {
    MOZ_ASSERT(displayClient);
    return;
  }
  const gfx::VRDisplayInfo& displayInfo = displayClient->GetDisplayInfo();
  const gfx::VRControllerState& controllerState =
      displayInfo.mControllerState[aIndex];
  MOZ_ASSERT(controllerState.controllerName[0] != '\0');

  mProfiles = GetInputSourceProfile(controllerState.type);
  mHandedness = XRHandedness::None;
  switch (controllerState.hand) {
    case GamepadHand::_empty:
      mHandedness = XRHandedness::None;
      break;
    case GamepadHand::Left:
      mHandedness = XRHandedness::Left;
      break;
    case GamepadHand::Right:
      mHandedness = XRHandedness::Right;
      break;
    default:
      MOZ_ASSERT(false && "Unknown GamepadHand type.");
      break;
  }

  RefPtr<XRNativeOrigin> nativeOriginTargetRay = nullptr;
  RefPtr<XRNativeOrigin> nativeOriginGrip = nullptr;
  mTargetRayMode = XRTargetRayMode::Tracked_pointer;
  switch (controllerState.targetRayMode) {
    case gfx::TargetRayMode::Gaze:
      mTargetRayMode = XRTargetRayMode::Gaze;
      nativeOriginTargetRay = new XRNativeOriginViewer(displayClient);
      nativeOriginGrip = new XRNativeOriginViewer(displayClient);
      break;
    case gfx::TargetRayMode::TrackedPointer:
      mTargetRayMode = XRTargetRayMode::Tracked_pointer;
      // We use weak pointers of poses in XRNativeOriginTracker to sync their
      // data internally.
      nativeOriginTargetRay =
          new XRNativeOriginTracker(&controllerState.targetRayPose);
      nativeOriginGrip = new XRNativeOriginTracker(&controllerState.pose);
      break;
    case gfx::TargetRayMode::Screen:
      mTargetRayMode = XRTargetRayMode::Screen;
      break;
    default:
      MOZ_ASSERT(false && "Undefined TargetRayMode type.");
      break;
  }
  mTargetRaySpace =
      new XRSpace(aSession->GetParentObject(), aSession, nativeOriginTargetRay);
  mGripSpace =
      new XRSpace(aSession->GetParentObject(), aSession, nativeOriginGrip);
  const uint32_t gamepadId =
      displayInfo.mDisplayID * kVRControllerMaxCount + aIndex;
  const uint32_t hashKey = GamepadManager::GetGamepadIndexWithServiceType(
      gamepadId, GamepadServiceType::VR);
  mGamepad =
      new Gamepad(mParent, NS_ConvertASCIItoUTF16(""), -1, hashKey,
                  GamepadMappingType::Xr_standard, controllerState.hand,
                  displayInfo.mDisplayID, controllerState.numButtons,
                  controllerState.numAxes, controllerState.numHaptics, 0, 0);
  mIndex = aIndex;
}

void XRInputSource::SetGamepadIsConnected(bool aConnected) {
  mGamepad->SetConnected(aConnected);
}

void XRInputSource::Update(XRSession* aSession) {
  MOZ_ASSERT(aSession && mIndex >= 0 && mGamepad);

  gfx::VRDisplayClient* displayClient = aSession->GetDisplayClient();
  if (!displayClient) {
    MOZ_ASSERT(displayClient);
    return;
  }
  const gfx::VRDisplayInfo& displayInfo = displayClient->GetDisplayInfo();
  const gfx::VRControllerState& controllerState =
      displayInfo.mControllerState[mIndex];
  MOZ_ASSERT(controllerState.controllerName[0] != '\0');

  // Update button values.
  nsTArray<RefPtr<GamepadButton>> buttons;
  mGamepad->GetButtons(buttons);
  for (uint32_t i = 0; i < buttons.Length(); ++i) {
    const bool pressed = controllerState.buttonPressed & (1ULL << i);
    const bool touched = controllerState.buttonTouched & (1ULL << i);

    if (buttons[i]->Pressed() != pressed || buttons[i]->Touched() != touched ||
        buttons[i]->Value() != controllerState.triggerValue[i]) {
      mGamepad->SetButton(i, pressed, touched, controllerState.triggerValue[i]);
    }
  }
  // Update axis values.
  nsTArray<double> axes;
  mGamepad->GetAxes(axes);
  for (uint32_t i = 0; i < axes.Length(); ++i) {
    if (axes[i] != controllerState.axisValue[i]) {
      mGamepad->SetAxis(i, controllerState.axisValue[i]);
    }
  }

  // We define 0.85f and 0.15f based on our current finding
  // for better experience, we can adjust these values if we need.
  const float completeThreshold = 0.90f;
  const float startThreshold = 0.85f;
  const float endThreshold = 0.15f;
  const uint32_t selectIndex = 0;
  const uint32_t squeezeIndex = 1;

  // Checking selectstart, select, selectend
  if (buttons.Length() > selectIndex) {
    if (controllerState.selectActionStartFrameId >
        controllerState.selectActionStopFrameId) {
      if (mSelectAction == ActionState::ActionState_Released &&
          controllerState.triggerValue[selectIndex] > endThreshold) {
        DispatchEvent(NS_LITERAL_STRING("selectstart"), aSession);
        mSelectAction = ActionState::ActionState_Pressing;
      } else if (mSelectAction == ActionState::ActionState_Pressing &&
                 controllerState.triggerValue[selectIndex] >
                     completeThreshold) {
        mSelectAction = ActionState::ActionState_Pressed;
      } else if (mSelectAction == ActionState::ActionState_Pressed &&
                 controllerState.triggerValue[selectIndex] < startThreshold) {
        DispatchEvent(NS_LITERAL_STRING("select"), aSession);
        mSelectAction = ActionState::ActionState_Releasing;
      } else if (mSelectAction <= ActionState::ActionState_Releasing &&
                 controllerState.triggerValue[selectIndex] < endThreshold) {
        DispatchEvent(NS_LITERAL_STRING("selectend"), aSession);
        mSelectAction = ActionState::ActionState_Released;
      }
    } else if (mSelectAction <= ActionState::ActionState_Releasing) {
      DispatchEvent(NS_LITERAL_STRING("selectend"), aSession);
      mSelectAction = ActionState::ActionState_Released;
    }
  }

  // Checking squeezestart, squeeze, squeezeend
  if (buttons.Length() > squeezeIndex) {
    if (controllerState.squeezeActionStartFrameId >
        controllerState.squeezeActionStopFrameId) {
      if (mSqueezeAction == ActionState::ActionState_Released &&
          controllerState.triggerValue[squeezeIndex] > endThreshold) {
        DispatchEvent(NS_LITERAL_STRING("squeezestart"), aSession);
        mSqueezeAction = ActionState::ActionState_Pressing;
      } else if (mSqueezeAction == ActionState::ActionState_Pressing &&
                 controllerState.triggerValue[squeezeIndex] >
                     completeThreshold) {
        mSqueezeAction = ActionState::ActionState_Pressed;
      } else if (mSqueezeAction == ActionState::ActionState_Pressed &&
                 controllerState.triggerValue[squeezeIndex] < startThreshold) {
        DispatchEvent(NS_LITERAL_STRING("squeeze"), aSession);
        mSqueezeAction = ActionState::ActionState_Releasing;
      } else if (mSqueezeAction <= ActionState::ActionState_Releasing &&
                 controllerState.triggerValue[squeezeIndex] < endThreshold) {
        DispatchEvent(NS_LITERAL_STRING("squeezeend"), aSession);
        mSqueezeAction = ActionState::ActionState_Released;
      }
    } else if (mSqueezeAction <= ActionState::ActionState_Releasing) {
      DispatchEvent(NS_LITERAL_STRING("squeezeend"), aSession);
      mSqueezeAction = ActionState::ActionState_Released;
    }
  }
}

int32_t XRInputSource::GetIndex() { return mIndex; }

void XRInputSource::DispatchEvent(const nsAString& aEvent,
                                  XRSession* aSession) {
  // Create a XRFrame for its callbacks
  RefPtr<XRFrame> frame = new XRFrame(GetParentObject(), aSession);
  frame->StartInputSourceEvent();

  XRInputSourceEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mFrame = frame;
  init.mInputSource = this;

  RefPtr<XRInputSourceEvent> event =
      XRInputSourceEvent::Constructor(aSession, aEvent, init);

  event->SetTrusted(true);
  aSession->DispatchEvent(*event);
  frame->EndInputSourceEvent();
}

}  // namespace dom
}  // namespace mozilla
