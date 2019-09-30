/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fstream>
#include "mozilla/JSONWriter.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsString.h"

#include "OpenVRSession.h"
#include "mozilla/StaticPrefs_dom.h"

#if defined(XP_WIN)
#  include <d3d11.h>
#  include "mozilla/gfx/DeviceManagerDx.h"
#elif defined(XP_MACOSX)
#  include "mozilla/gfx/MacIOSurface.h"
#endif

#if !defined(XP_WIN)
#  include <sys/stat.h>  // for umask()
#endif

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "binding/OpenVRCosmosBinding.h"
#include "binding/OpenVRKnucklesBinding.h"
#include "binding/OpenVRViveBinding.h"
#if defined(XP_WIN)  // Windows Mixed Reality is only available in Windows.
#  include "binding/OpenVRWMRBinding.h"
#endif

#include "VRParent.h"
#include "VRProcessChild.h"
#include "VRThread.h"

#if !defined(M_PI)
#  define M_PI 3.14159265358979323846264338327950288
#endif

#define BTN_MASK_FROM_ID(_id) ::vr::ButtonMaskFromId(vr::EVRButtonId::_id)

// Haptic feedback is updated every 5ms, as this is
// the minimum period between new haptic pulse requests.
// Effectively, this results in a pulse width modulation
// with an interval of 5ms.  Through experimentation, the
// maximum duty cycle was found to be about 3.9ms
const uint32_t kVRHapticUpdateInterval = 5;

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

namespace {

// This is for controller action file writer.
struct StringWriteFunc : public JSONWriteFunc {
  nsACString& mBuffer;  // This struct must not outlive this buffer

  explicit StringWriteFunc(nsACString& buffer) : mBuffer(buffer) {}

  void Write(const char* aStr) override { mBuffer.Append(aStr); }
};

class ControllerManifestFile {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ControllerManifestFile)

 public:
  static already_AddRefed<ControllerManifestFile> CreateManifest() {
    RefPtr<ControllerManifestFile> manifest = new ControllerManifestFile();
    return manifest.forget();
  }

  bool IsExisting() {
    if (mFileName.IsEmpty() ||
        !std::ifstream(mFileName.BeginReading()).good()) {
      return false;
    }
    return true;
  }

  void SetFileName(const char* aName) { mFileName = aName; }

  const char* GetFileName() const { return mFileName.BeginReading(); }

 private:
  ControllerManifestFile() = default;

  ~ControllerManifestFile() {
    if (!mFileName.IsEmpty() && remove(mFileName.BeginReading()) != 0) {
      MOZ_ASSERT(false, "Delete controller manifest file failed.");
    }
    mFileName = "";
  }

  nsCString mFileName;
};

// We wanna keep these temporary files existing
// until Firefox is closed instead of following OpenVRSession's lifetime.
StaticRefPtr<ControllerManifestFile> sCosmosBindingFile;
StaticRefPtr<ControllerManifestFile> sKnucklesBindingFile;
StaticRefPtr<ControllerManifestFile> sViveBindingFile;
#if defined(XP_WIN)
StaticRefPtr<ControllerManifestFile> sWMRBindingFile;
#endif
StaticRefPtr<ControllerManifestFile> sControllerActionFile;

dom::GamepadHand GetControllerHandFromControllerRole(
    ::vr::ETrackedControllerRole aRole) {
  dom::GamepadHand hand;

  switch (aRole) {
    case ::vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
    case ::vr::ETrackedControllerRole::TrackedControllerRole_OptOut:
      hand = dom::GamepadHand::_empty;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
      hand = dom::GamepadHand::Left;
      break;
    case ::vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
      hand = dom::GamepadHand::Right;
      break;
    default:
      hand = dom::GamepadHand::_empty;
      MOZ_ASSERT(false);
      break;
  }

  return hand;
}

dom::GamepadHand GetControllerHandFromControllerRole(OpenVRHand aRole) {
  dom::GamepadHand hand;
  switch (aRole) {
    case OpenVRHand::None:
      hand = dom::GamepadHand::_empty;
      break;
    case OpenVRHand::Left:
      hand = dom::GamepadHand::Left;
      break;
    case OpenVRHand::Right:
      hand = dom::GamepadHand::Right;
      break;
    default:
      hand = dom::GamepadHand::_empty;
      MOZ_ASSERT(false);
      break;
  }

  return hand;
}

void UpdateButton(VRControllerState& aState,
                  const ::vr::VRControllerState_t& aControllerState,
                  uint32_t aButtonIndex, uint64_t aButtonMask) {
  uint64_t mask = (1ULL << aButtonIndex);
  if ((aControllerState.ulButtonPressed & aButtonMask) == 0) {
    // not pressed
    aState.buttonPressed &= ~mask;
    aState.triggerValue[aButtonIndex] = 0.0f;
  } else {
    // pressed
    aState.buttonPressed |= mask;
    aState.triggerValue[aButtonIndex] = 1.0f;
  }
  if ((aControllerState.ulButtonTouched & aButtonMask) == 0) {
    // not touched
    aState.buttonTouched &= ~mask;
  } else {
    // touched
    aState.buttonTouched |= mask;
  }
}

bool FileIsExisting(const nsCString& aPath) {
  if (aPath.IsEmpty() || !std::ifstream(aPath.BeginReading()).good()) {
    return false;
  }
  return true;
}

};  // anonymous namespace

#if defined(XP_WIN)
bool GenerateTempFileName(nsCString& aPath) {
  TCHAR tempPathBuffer[MAX_PATH];
  TCHAR tempFileName[MAX_PATH];

  // Gets the temp path env string (no guarantee it's a valid path).
  DWORD dwRetVal = GetTempPath(MAX_PATH, tempPathBuffer);
  if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
    NS_WARNING("OpenVR - Creating temp path failed.");
    return false;
  }

  // Generates a temporary file name.
  UINT uRetVal = GetTempFileName(tempPathBuffer,  // directory for tmp files
                                 TEXT("mozvr"),   // temp file name prefix
                                 0,               // create unique name
                                 tempFileName);   // buffer for name
  if (uRetVal == 0) {
    NS_WARNING("OpenVR - Creating temp file failed.");
    return false;
  }

  aPath.Assign(NS_ConvertUTF16toUTF8(tempFileName));
  return true;
}
#else
bool GenerateTempFileName(nsCString& aPath) {
  const char tmp[] = "/tmp/mozvrXXXXXX";
  char fileName[PATH_MAX];

  strcpy(fileName, tmp);
  const mode_t prevMask = umask(S_IXUSR | S_IRWXO | S_IRWXG);
  const int fd = mkstemp(fileName);
  umask(prevMask);
  if (fd == -1) {
    NS_WARNING(nsPrintfCString("OpenVR - Creating temp file failed: %s",
                               strerror(errno))
                   .get());
    return false;
  }
  close(fd);

  aPath.Assign(fileName);
  return true;
}
#endif  // defined(XP_WIN)

OpenVRSession::OpenVRSession()
    : VRSession(),
      mVRSystem(nullptr),
      mVRChaperone(nullptr),
      mVRCompositor(nullptr),
      mControllerDeviceIndexObsolete{},
      mHapticPulseRemaining{},
      mHapticPulseIntensity{},
      mIsWindowsMR(false),
      mControllerHapticStateMutex(
          "OpenVRSession::mControllerHapticStateMutex") {
  std::fill_n(mControllerDeviceIndex, kVRControllerMaxCount, OpenVRHand::None);
}

OpenVRSession::~OpenVRSession() {
  mActionsetFirefox = ::vr::k_ulInvalidActionSetHandle;
  Shutdown();
}

bool OpenVRSession::Initialize(mozilla::gfx::VRSystemState& aSystemState) {
  if (!StaticPrefs::dom_vr_enabled() ||
      !StaticPrefs::dom_vr_openvr_enabled_AtStartup()) {
    return false;
  }
  if (mVRSystem != nullptr) {
    // Already initialized
    return true;
  }
  if (!::vr::VR_IsRuntimeInstalled()) {
    return false;
  }
  if (!::vr::VR_IsHmdPresent()) {
    return false;
  }

  ::vr::HmdError err;

  ::vr::VR_Init(&err, ::vr::EVRApplicationType::VRApplication_Scene);
  if (err) {
    return false;
  }

  mVRSystem = (::vr::IVRSystem*)::vr::VR_GetGenericInterface(
      ::vr::IVRSystem_Version, &err);
  if (err || !mVRSystem) {
    Shutdown();
    return false;
  }
  mVRChaperone = (::vr::IVRChaperone*)::vr::VR_GetGenericInterface(
      ::vr::IVRChaperone_Version, &err);
  if (err || !mVRChaperone) {
    Shutdown();
    return false;
  }
  mVRCompositor = (::vr::IVRCompositor*)::vr::VR_GetGenericInterface(
      ::vr::IVRCompositor_Version, &err);
  if (err || !mVRCompositor) {
    Shutdown();
    return false;
  }

#if defined(XP_WIN)
  if (!CreateD3DObjects()) {
    Shutdown();
    return false;
  }

#endif

  // Configure coordinate system
  mVRCompositor->SetTrackingSpace(::vr::TrackingUniverseSeated);

  if (!InitState(aSystemState)) {
    Shutdown();
    return false;
  }

  if (StaticPrefs::dom_vr_openvr_action_input_AtStartup() &&
      !SetupContollerActions()) {
    return false;
  }

  // Succeeded
  return true;
}

bool OpenVRSession::SetupContollerActions() {
  if (!vr::VRInput()) {
    NS_WARNING("OpenVR - vr::VRInput() is null.");
    return false;
  }

  // Check if this device binding file has been created.
  // If it didn't exist yet, create a new temp file.
  nsCString controllerAction;
  nsCString viveManifest;
  nsCString WMRManifest;
  nsCString knucklesManifest;
  nsCString cosmosManifest;

  // Getting / Generating manifest file paths.
  if (StaticPrefs::dom_vr_process_enabled_AtStartup()) {
    VRParent* vrParent = VRProcessChild::GetVRParent();
    nsCString output;

    if (vrParent->GetOpenVRControllerActionPath(&output)) {
      controllerAction = output;
    }

    if (vrParent->GetOpenVRControllerManifestPath(OpenVRControllerType::Vive,
                                                  &output)) {
      viveManifest = output;
    }
    if (!viveManifest.Length() || !FileIsExisting(viveManifest)) {
      if (!GenerateTempFileName(viveManifest)) {
        return false;
      }
      OpenVRViveBinding viveBinding;
      std::ofstream viveBindingFile(viveManifest.BeginReading());
      if (viveBindingFile.is_open()) {
        viveBindingFile << viveBinding.binding;
        viveBindingFile.close();
      }
    }

#if defined(XP_WIN)
    if (vrParent->GetOpenVRControllerManifestPath(OpenVRControllerType::WMR,
                                                  &output)) {
      WMRManifest = output;
    }
    if (!WMRManifest.Length() || !FileIsExisting(WMRManifest)) {
      if (!GenerateTempFileName(WMRManifest)) {
        return false;
      }
      OpenVRWMRBinding WMRBinding;
      std::ofstream WMRBindingFile(WMRManifest.BeginReading());
      if (WMRBindingFile.is_open()) {
        WMRBindingFile << WMRBinding.binding;
        WMRBindingFile.close();
      }
    }
#endif
    if (vrParent->GetOpenVRControllerManifestPath(
            OpenVRControllerType::Knuckles, &output)) {
      knucklesManifest = output;
    }
    if (!knucklesManifest.Length() || !FileIsExisting(knucklesManifest)) {
      if (!GenerateTempFileName(knucklesManifest)) {
        return false;
      }
      OpenVRKnucklesBinding knucklesBinding;
      std::ofstream knucklesBindingFile(knucklesManifest.BeginReading());
      if (knucklesBindingFile.is_open()) {
        knucklesBindingFile << knucklesBinding.binding;
        knucklesBindingFile.close();
      }
    }
    if (vrParent->GetOpenVRControllerManifestPath(OpenVRControllerType::Cosmos,
                                                  &output)) {
      cosmosManifest = output;
    }
    if (!cosmosManifest.Length() || !FileIsExisting(cosmosManifest)) {
      if (!GenerateTempFileName(cosmosManifest)) {
        return false;
      }
      OpenVRCosmosBinding cosmosBinding;
      std::ofstream cosmosBindingFile(cosmosManifest.BeginReading());
      if (cosmosBindingFile.is_open()) {
        cosmosBindingFile << cosmosBinding.binding;
        cosmosBindingFile.close();
      }
    }
  } else {
    // Without using VR process
    if (!sControllerActionFile) {
      sControllerActionFile = ControllerManifestFile::CreateManifest();
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ClearOnShutdown ControllerManifestFile",
          []() { ClearOnShutdown(&sControllerActionFile); }));
    }
    controllerAction = sControllerActionFile->GetFileName();

    if (!sViveBindingFile) {
      sViveBindingFile = ControllerManifestFile::CreateManifest();
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("ClearOnShutdown ControllerManifestFile",
                                 []() { ClearOnShutdown(&sViveBindingFile); }));
    }
    if (!sViveBindingFile->IsExisting()) {
      nsCString viveBindingPath;
      if (!GenerateTempFileName(viveBindingPath)) {
        return false;
      }
      sViveBindingFile->SetFileName(viveBindingPath.BeginReading());
      OpenVRViveBinding viveBinding;
      std::ofstream viveBindingFile(sViveBindingFile->GetFileName());
      if (viveBindingFile.is_open()) {
        viveBindingFile << viveBinding.binding;
        viveBindingFile.close();
      }
    }
    viveManifest = sViveBindingFile->GetFileName();

    if (!sKnucklesBindingFile) {
      sKnucklesBindingFile = ControllerManifestFile::CreateManifest();
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ClearOnShutdown ControllerManifestFile",
          []() { ClearOnShutdown(&sKnucklesBindingFile); }));
    }
    if (!sKnucklesBindingFile->IsExisting()) {
      nsCString knucklesBindingPath;
      if (!GenerateTempFileName(knucklesBindingPath)) {
        return false;
      }
      sKnucklesBindingFile->SetFileName(knucklesBindingPath.BeginReading());
      OpenVRKnucklesBinding knucklesBinding;
      std::ofstream knucklesBindingFile(sKnucklesBindingFile->GetFileName());
      if (knucklesBindingFile.is_open()) {
        knucklesBindingFile << knucklesBinding.binding;
        knucklesBindingFile.close();
      }
    }
    knucklesManifest = sKnucklesBindingFile->GetFileName();

    if (!sCosmosBindingFile) {
      sCosmosBindingFile = ControllerManifestFile::CreateManifest();
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ClearOnShutdown ControllerManifestFile",
          []() { ClearOnShutdown(&sCosmosBindingFile); }));
    }
    if (!sCosmosBindingFile->IsExisting()) {
      nsCString cosmosBindingPath;
      if (!GenerateTempFileName(cosmosBindingPath)) {
        return false;
      }
      sCosmosBindingFile->SetFileName(cosmosBindingPath.BeginReading());
      OpenVRCosmosBinding cosmosBinding;
      std::ofstream cosmosBindingFile(sCosmosBindingFile->GetFileName());
      if (cosmosBindingFile.is_open()) {
        cosmosBindingFile << cosmosBinding.binding;
        cosmosBindingFile.close();
      }
    }
    cosmosManifest = sCosmosBindingFile->GetFileName();
#if defined(XP_WIN)
    if (!sWMRBindingFile) {
      sWMRBindingFile = ControllerManifestFile::CreateManifest();
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("ClearOnShutdown ControllerManifestFile",
                                 []() { ClearOnShutdown(&sWMRBindingFile); }));
    }
    if (!sWMRBindingFile->IsExisting()) {
      nsCString WMRBindingPath;
      if (!GenerateTempFileName(WMRBindingPath)) {
        return false;
      }
      sWMRBindingFile->SetFileName(WMRBindingPath.BeginReading());
      OpenVRWMRBinding WMRBinding;
      std::ofstream WMRBindingFile(sWMRBindingFile->GetFileName());
      if (WMRBindingFile.is_open()) {
        WMRBindingFile << WMRBinding.binding;
        WMRBindingFile.close();
      }
    }
    WMRManifest = sWMRBindingFile->GetFileName();
#endif
  }
  // End of Getting / Generating manifest file paths.

  // Setup controller actions.
  ControllerInfo leftContollerInfo;
  leftContollerInfo.mActionPose =
      ControllerAction("/actions/firefox/in/LHand_pose", "pose");
  leftContollerInfo.mActionHaptic =
      ControllerAction("/actions/firefox/out/LHand_haptic", "vibration");
  leftContollerInfo.mActionTrackpad_Analog =
      ControllerAction("/actions/firefox/in/LHand_trackpad_analog", "vector2");
  leftContollerInfo.mActionTrackpad_Pressed =
      ControllerAction("/actions/firefox/in/LHand_trackpad_pressed", "boolean");
  leftContollerInfo.mActionTrackpad_Touched =
      ControllerAction("/actions/firefox/in/LHand_trackpad_touched", "boolean");
  leftContollerInfo.mActionTrigger_Value =
      ControllerAction("/actions/firefox/in/LHand_trigger_value", "vector1");
  leftContollerInfo.mActionGrip_Pressed =
      ControllerAction("/actions/firefox/in/LHand_grip_pressed", "boolean");
  leftContollerInfo.mActionGrip_Touched =
      ControllerAction("/actions/firefox/in/LHand_grip_touched", "boolean");
  leftContollerInfo.mActionMenu_Pressed =
      ControllerAction("/actions/firefox/in/LHand_menu_pressed", "boolean");
  leftContollerInfo.mActionMenu_Touched =
      ControllerAction("/actions/firefox/in/LHand_menu_touched", "boolean");
  leftContollerInfo.mActionSystem_Pressed =
      ControllerAction("/actions/firefox/in/LHand_system_pressed", "boolean");
  leftContollerInfo.mActionSystem_Touched =
      ControllerAction("/actions/firefox/in/LHand_system_touched", "boolean");
  leftContollerInfo.mActionA_Pressed =
      ControllerAction("/actions/firefox/in/LHand_A_pressed", "boolean");
  leftContollerInfo.mActionA_Touched =
      ControllerAction("/actions/firefox/in/LHand_A_touched", "boolean");
  leftContollerInfo.mActionB_Pressed =
      ControllerAction("/actions/firefox/in/LHand_B_pressed", "boolean");
  leftContollerInfo.mActionB_Touched =
      ControllerAction("/actions/firefox/in/LHand_B_touched", "boolean");
  leftContollerInfo.mActionThumbstick_Analog = ControllerAction(
      "/actions/firefox/in/LHand_thumbstick_analog", "vector2");
  leftContollerInfo.mActionThumbstick_Pressed = ControllerAction(
      "/actions/firefox/in/LHand_thumbstick_pressed", "boolean");
  leftContollerInfo.mActionThumbstick_Touched = ControllerAction(
      "/actions/firefox/in/LHand_thumbstick_touched", "boolean");
  leftContollerInfo.mActionFingerIndex_Value = ControllerAction(
      "/actions/firefox/in/LHand_finger_index_value", "vector1");
  leftContollerInfo.mActionFingerMiddle_Value = ControllerAction(
      "/actions/firefox/in/LHand_finger_middle_value", "vector1");
  leftContollerInfo.mActionFingerRing_Value = ControllerAction(
      "/actions/firefox/in/LHand_finger_ring_value", "vector1");
  leftContollerInfo.mActionFingerPinky_Value = ControllerAction(
      "/actions/firefox/in/LHand_finger_pinky_value", "vector1");
  leftContollerInfo.mActionBumper_Pressed =
      ControllerAction("/actions/firefox/in/LHand_bumper_pressed", "boolean");

  ControllerInfo rightContollerInfo;
  rightContollerInfo.mActionPose =
      ControllerAction("/actions/firefox/in/RHand_pose", "pose");
  rightContollerInfo.mActionHaptic =
      ControllerAction("/actions/firefox/out/RHand_haptic", "vibration");
  rightContollerInfo.mActionTrackpad_Analog =
      ControllerAction("/actions/firefox/in/RHand_trackpad_analog", "vector2");
  rightContollerInfo.mActionTrackpad_Pressed =
      ControllerAction("/actions/firefox/in/RHand_trackpad_pressed", "boolean");
  rightContollerInfo.mActionTrackpad_Touched =
      ControllerAction("/actions/firefox/in/RHand_trackpad_touched", "boolean");
  rightContollerInfo.mActionTrigger_Value =
      ControllerAction("/actions/firefox/in/RHand_trigger_value", "vector1");
  rightContollerInfo.mActionGrip_Pressed =
      ControllerAction("/actions/firefox/in/RHand_grip_pressed", "boolean");
  rightContollerInfo.mActionGrip_Touched =
      ControllerAction("/actions/firefox/in/RHand_grip_touched", "boolean");
  rightContollerInfo.mActionMenu_Pressed =
      ControllerAction("/actions/firefox/in/RHand_menu_pressed", "boolean");
  rightContollerInfo.mActionMenu_Touched =
      ControllerAction("/actions/firefox/in/RHand_menu_touched", "boolean");
  rightContollerInfo.mActionSystem_Pressed =
      ControllerAction("/actions/firefox/in/RHand_system_pressed", "boolean");
  rightContollerInfo.mActionSystem_Touched =
      ControllerAction("/actions/firefox/in/RHand_system_touched", "boolean");
  rightContollerInfo.mActionA_Pressed =
      ControllerAction("/actions/firefox/in/RHand_A_pressed", "boolean");
  rightContollerInfo.mActionA_Touched =
      ControllerAction("/actions/firefox/in/RHand_A_touched", "boolean");
  rightContollerInfo.mActionB_Pressed =
      ControllerAction("/actions/firefox/in/RHand_B_pressed", "boolean");
  rightContollerInfo.mActionB_Touched =
      ControllerAction("/actions/firefox/in/RHand_B_touched", "boolean");
  rightContollerInfo.mActionThumbstick_Analog = ControllerAction(
      "/actions/firefox/in/RHand_thumbstick_analog", "vector2");
  rightContollerInfo.mActionThumbstick_Pressed = ControllerAction(
      "/actions/firefox/in/RHand_thumbstick_pressed", "boolean");
  rightContollerInfo.mActionThumbstick_Touched = ControllerAction(
      "/actions/firefox/in/RHand_thumbstick_touched", "boolean");
  rightContollerInfo.mActionFingerIndex_Value = ControllerAction(
      "/actions/firefox/in/RHand_finger_index_value", "vector1");
  rightContollerInfo.mActionFingerMiddle_Value = ControllerAction(
      "/actions/firefox/in/RHand_finger_middle_value", "vector1");
  rightContollerInfo.mActionFingerRing_Value = ControllerAction(
      "/actions/firefox/in/RHand_finger_ring_value", "vector1");
  rightContollerInfo.mActionFingerPinky_Value = ControllerAction(
      "/actions/firefox/in/RHand_finger_pinky_value", "vector1");
  rightContollerInfo.mActionBumper_Pressed =
      ControllerAction("/actions/firefox/in/RHand_bumper_pressed", "boolean");

  mControllerHand[OpenVRHand::Left] = leftContollerInfo;
  mControllerHand[OpenVRHand::Right] = rightContollerInfo;

  if (!controllerAction.Length() || !FileIsExisting(controllerAction)) {
    if (!GenerateTempFileName(controllerAction)) {
      return false;
    }
    nsCString actionData;
    JSONWriter actionWriter(MakeUnique<StringWriteFunc>(actionData));
    actionWriter.Start();

    actionWriter.StringProperty("version",
                                "0.1.0");  // TODO: adding a version check.
    // "default_bindings": []
    actionWriter.StartArrayProperty("default_bindings");
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "vive_controller");
    actionWriter.StringProperty("binding_url", viveManifest.BeginReading());
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "knuckles");
    actionWriter.StringProperty("binding_url", knucklesManifest.BeginReading());
    actionWriter.EndObject();
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "vive_cosmos_controller");
    actionWriter.StringProperty("binding_url", cosmosManifest.BeginReading());
    actionWriter.EndObject();
#if defined(XP_WIN)
    actionWriter.StartObjectElement();
    actionWriter.StringProperty("controller_type", "holographic_controller");
    actionWriter.StringProperty("binding_url", WMRManifest.BeginReading());
    actionWriter.EndObject();
#endif
    actionWriter.EndArray();  // End "default_bindings": []

    // "actions": [] Action paths must take the form: "/actions/<action
    // set>/in|out/<action>"
    actionWriter.StartArrayProperty("actions");

    for (auto& controller : mControllerHand) {
      actionWriter.StartObjectElement();
      actionWriter.StringProperty("name",
                                  controller.mActionPose.name.BeginReading());
      actionWriter.StringProperty("type",
                                  controller.mActionPose.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionTrackpad_Analog.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionTrackpad_Analog.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionTrackpad_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionTrackpad_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionTrackpad_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionTrackpad_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionTrigger_Value.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionTrigger_Value.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionGrip_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionGrip_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionGrip_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionGrip_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionMenu_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionMenu_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionMenu_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionMenu_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionSystem_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionSystem_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionSystem_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionSystem_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionA_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionA_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionA_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionA_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionB_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionB_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionB_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionB_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionThumbstick_Analog.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionThumbstick_Analog.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionThumbstick_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionThumbstick_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionThumbstick_Touched.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionThumbstick_Touched.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionFingerIndex_Value.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionFingerIndex_Value.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionFingerMiddle_Value.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionFingerMiddle_Value.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionFingerRing_Value.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionFingerRing_Value.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionFingerPinky_Value.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionFingerPinky_Value.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty(
          "name", controller.mActionBumper_Pressed.name.BeginReading());
      actionWriter.StringProperty(
          "type", controller.mActionBumper_Pressed.type.BeginReading());
      actionWriter.EndObject();

      actionWriter.StartObjectElement();
      actionWriter.StringProperty("name",
                                  controller.mActionHaptic.name.BeginReading());
      actionWriter.StringProperty("type",
                                  controller.mActionHaptic.type.BeginReading());
      actionWriter.EndObject();
    }
    actionWriter.EndArray();  // End "actions": []
    actionWriter.End();

    std::ofstream actionfile(controllerAction.BeginReading());
    nsCString actionResult(actionData.get());
    if (actionfile.is_open()) {
      actionfile << actionResult.get();
      actionfile.close();
    }
  }

  vr::EVRInputError err =
      vr::VRInput()->SetActionManifestPath(controllerAction.BeginReading());
  if (err != vr::VRInputError_None) {
    NS_WARNING("OpenVR - SetActionManifestPath failed.");
    return false;
  }
  // End of setup controller actions.

  // Notify the parent process these manifest files are already been recorded.
  if (StaticPrefs::dom_vr_process_enabled_AtStartup()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "SendOpenVRControllerActionPathToParent",
        [controllerAction, viveManifest, WMRManifest, knucklesManifest,
         cosmosManifest]() {
          VRParent* vrParent = VRProcessChild::GetVRParent();
          Unused << vrParent->SendOpenVRControllerActionPathToParent(
              controllerAction);
          Unused << vrParent->SendOpenVRControllerManifestPathToParent(
              OpenVRControllerType::Vive, viveManifest);
          Unused << vrParent->SendOpenVRControllerManifestPathToParent(
              OpenVRControllerType::WMR, WMRManifest);
          Unused << vrParent->SendOpenVRControllerManifestPathToParent(
              OpenVRControllerType::Knuckles, knucklesManifest);
          Unused << vrParent->SendOpenVRControllerManifestPathToParent(
              OpenVRControllerType::Cosmos, cosmosManifest);
        }));
  } else {
    sControllerActionFile->SetFileName(controllerAction.BeginReading());
  }

  return true;
}

#if defined(XP_WIN)
bool OpenVRSession::CreateD3DObjects() {
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetVRDevice();
  if (!device) {
    return false;
  }
  if (!CreateD3DContext(device)) {
    return false;
  }
  return true;
}
#endif

void OpenVRSession::Shutdown() {
  StopHapticTimer();
  StopHapticThread();
  if (mVRSystem || mVRCompositor || mVRSystem) {
    ::vr::VR_Shutdown();
    mVRCompositor = nullptr;
    mVRChaperone = nullptr;
    mVRSystem = nullptr;
  }
}

bool OpenVRSession::InitState(VRSystemState& aSystemState) {
  VRDisplayState& state = aSystemState.displayState;
  strncpy(state.displayName, "OpenVR HMD", kVRDisplayNameMaxLen);
  state.eightCC = GFX_VR_EIGHTCC('O', 'p', 'e', 'n', 'V', 'R', ' ', ' ');
  state.isConnected =
      mVRSystem->IsTrackedDeviceConnected(::vr::k_unTrackedDeviceIndex_Hmd);
  state.isMounted = false;
  state.capabilityFlags = (VRDisplayCapabilityFlags)(
      (int)VRDisplayCapabilityFlags::Cap_None |
      (int)VRDisplayCapabilityFlags::Cap_Orientation |
      (int)VRDisplayCapabilityFlags::Cap_Position |
      (int)VRDisplayCapabilityFlags::Cap_External |
      (int)VRDisplayCapabilityFlags::Cap_Present |
      (int)VRDisplayCapabilityFlags::Cap_StageParameters |
      (int)VRDisplayCapabilityFlags::Cap_ImmersiveVR);
  state.blendMode = VRDisplayBlendMode::Opaque;
  state.reportsDroppedFrames = true;

  ::vr::ETrackedPropertyError err;
  bool bHasProximitySensor = mVRSystem->GetBoolTrackedDeviceProperty(
      ::vr::k_unTrackedDeviceIndex_Hmd, ::vr::Prop_ContainsProximitySensor_Bool,
      &err);
  if (err == ::vr::TrackedProp_Success && bHasProximitySensor) {
    state.capabilityFlags = (VRDisplayCapabilityFlags)(
        (int)state.capabilityFlags |
        (int)VRDisplayCapabilityFlags::Cap_MountDetection);
  }

  uint32_t w, h;
  mVRSystem->GetRecommendedRenderTargetSize(&w, &h);
  state.eyeResolution.width = w;
  state.eyeResolution.height = h;

  // default to an identity quaternion
  aSystemState.sensorState.pose.orientation[3] = 1.0f;

  UpdateStageParameters(state);
  UpdateEyeParameters(aSystemState);

  VRHMDSensorState& sensorState = aSystemState.sensorState;
  sensorState.flags = (VRDisplayCapabilityFlags)(
      (int)VRDisplayCapabilityFlags::Cap_Orientation |
      (int)VRDisplayCapabilityFlags::Cap_Position);
  sensorState.pose.orientation[3] = 1.0f;  // Default to an identity quaternion

  return true;
}

void OpenVRSession::UpdateStageParameters(VRDisplayState& aState) {
  float sizeX = 0.0f;
  float sizeZ = 0.0f;
  if (mVRChaperone->GetPlayAreaSize(&sizeX, &sizeZ)) {
    ::vr::HmdMatrix34_t t =
        mVRSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
    aState.stageSize.width = sizeX;
    aState.stageSize.height = sizeZ;

    aState.sittingToStandingTransform[0] = t.m[0][0];
    aState.sittingToStandingTransform[1] = t.m[1][0];
    aState.sittingToStandingTransform[2] = t.m[2][0];
    aState.sittingToStandingTransform[3] = 0.0f;

    aState.sittingToStandingTransform[4] = t.m[0][1];
    aState.sittingToStandingTransform[5] = t.m[1][1];
    aState.sittingToStandingTransform[6] = t.m[2][1];
    aState.sittingToStandingTransform[7] = 0.0f;

    aState.sittingToStandingTransform[8] = t.m[0][2];
    aState.sittingToStandingTransform[9] = t.m[1][2];
    aState.sittingToStandingTransform[10] = t.m[2][2];
    aState.sittingToStandingTransform[11] = 0.0f;

    aState.sittingToStandingTransform[12] = t.m[0][3];
    aState.sittingToStandingTransform[13] = t.m[1][3];
    aState.sittingToStandingTransform[14] = t.m[2][3];
    aState.sittingToStandingTransform[15] = 1.0f;
  } else {
    // If we fail, fall back to reasonable defaults.
    // 1m x 1m space, 0.75m high in seated position
    aState.stageSize.width = 1.0f;
    aState.stageSize.height = 1.0f;

    aState.sittingToStandingTransform[0] = 1.0f;
    aState.sittingToStandingTransform[1] = 0.0f;
    aState.sittingToStandingTransform[2] = 0.0f;
    aState.sittingToStandingTransform[3] = 0.0f;

    aState.sittingToStandingTransform[4] = 0.0f;
    aState.sittingToStandingTransform[5] = 1.0f;
    aState.sittingToStandingTransform[6] = 0.0f;
    aState.sittingToStandingTransform[7] = 0.0f;

    aState.sittingToStandingTransform[8] = 0.0f;
    aState.sittingToStandingTransform[9] = 0.0f;
    aState.sittingToStandingTransform[10] = 1.0f;
    aState.sittingToStandingTransform[11] = 0.0f;

    aState.sittingToStandingTransform[12] = 0.0f;
    aState.sittingToStandingTransform[13] = 0.75f;
    aState.sittingToStandingTransform[14] = 0.0f;
    aState.sittingToStandingTransform[15] = 1.0f;
  }
}

void OpenVRSession::UpdateEyeParameters(VRSystemState& aState) {
  // This must be called every frame in order to
  // account for continuous adjustments to ipd.
  gfx::Matrix4x4 headToEyeTransforms[2];

  for (uint32_t eye = 0; eye < 2; ++eye) {
    ::vr::HmdMatrix34_t eyeToHead =
        mVRSystem->GetEyeToHeadTransform(static_cast<::vr::Hmd_Eye>(eye));
    aState.displayState.eyeTranslation[eye].x = eyeToHead.m[0][3];
    aState.displayState.eyeTranslation[eye].y = eyeToHead.m[1][3];
    aState.displayState.eyeTranslation[eye].z = eyeToHead.m[2][3];

    float left, right, up, down;
    mVRSystem->GetProjectionRaw(static_cast<::vr::Hmd_Eye>(eye), &left, &right,
                                &up, &down);
    aState.displayState.eyeFOV[eye].upDegrees = atan(-up) * 180.0 / M_PI;
    aState.displayState.eyeFOV[eye].rightDegrees = atan(right) * 180.0 / M_PI;
    aState.displayState.eyeFOV[eye].downDegrees = atan(down) * 180.0 / M_PI;
    aState.displayState.eyeFOV[eye].leftDegrees = atan(-left) * 180.0 / M_PI;

    Matrix4x4 pose;
    // NOTE! eyeToHead.m is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.
    memcpy(&pose._11, &eyeToHead.m, sizeof(eyeToHead.m));
    pose.Transpose();
    pose.Invert();
    headToEyeTransforms[eye] = pose;
  }
  aState.sensorState.CalcViewMatrices(headToEyeTransforms);
}

void OpenVRSession::UpdateHeadsetPose(VRSystemState& aState) {
  const uint32_t posesSize = ::vr::k_unTrackedDeviceIndex_Hmd + 1;
  ::vr::TrackedDevicePose_t poses[posesSize];
  // Note: We *must* call WaitGetPoses in order for any rendering to happen at
  // all.
  mVRCompositor->WaitGetPoses(poses, posesSize, nullptr, 0);

  ::vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(::vr::Compositor_FrameTiming);
  if (mVRCompositor->GetFrameTiming(&timing)) {
    aState.sensorState.timestamp = timing.m_flSystemTimeInSeconds;
  } else {
    // This should not happen, but log it just in case
    fprintf(stderr, "OpenVR - IVRCompositor::GetFrameTiming failed");
  }

  if (poses[::vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid &&
      poses[::vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult ==
          ::vr::TrackingResult_Running_OK) {
    const ::vr::TrackedDevicePose_t& pose =
        poses[::vr::k_unTrackedDeviceIndex_Hmd];

    gfx::Matrix4x4 m;
    // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.  We do this so we can
    // pull out a Quaternion.
    memcpy(&m._11, &pose.mDeviceToAbsoluteTracking,
           sizeof(pose.mDeviceToAbsoluteTracking));
    m.Transpose();

    gfx::Quaternion rot;
    rot.SetFromRotationMatrix(m);
    rot.Invert();

    aState.sensorState.flags = (VRDisplayCapabilityFlags)(
        (int)aState.sensorState.flags |
        (int)VRDisplayCapabilityFlags::Cap_Orientation);
    aState.sensorState.pose.orientation[0] = rot.x;
    aState.sensorState.pose.orientation[1] = rot.y;
    aState.sensorState.pose.orientation[2] = rot.z;
    aState.sensorState.pose.orientation[3] = rot.w;
    aState.sensorState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
    aState.sensorState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
    aState.sensorState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];

    aState.sensorState.flags =
        (VRDisplayCapabilityFlags)((int)aState.sensorState.flags |
                                   (int)VRDisplayCapabilityFlags::Cap_Position);
    aState.sensorState.pose.position[0] = m._41;
    aState.sensorState.pose.position[1] = m._42;
    aState.sensorState.pose.position[2] = m._43;
    aState.sensorState.pose.linearVelocity[0] = pose.vVelocity.v[0];
    aState.sensorState.pose.linearVelocity[1] = pose.vVelocity.v[1];
    aState.sensorState.pose.linearVelocity[2] = pose.vVelocity.v[2];
  }
}

void OpenVRSession::EnumerateControllers(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  bool controllerPresent[kVRControllerMaxCount] = {false};
  uint32_t stateIndex = 0;
  mActionsetFirefox = vr::k_ulInvalidActionSetHandle;

  if (vr::VRInput()->GetActionSetHandle(
          "/actions/firefox", &mActionsetFirefox) != vr::VRInputError_None) {
    return;
  }

  for (int8_t handIndex = 0; handIndex < OpenVRHand::Total; ++handIndex) {
    if (handIndex == OpenVRHand::Left) {
      if (vr::VRInput()->GetInputSourceHandle(
              "/user/hand/left", &mControllerHand[OpenVRHand::Left].mSource) !=
          vr::VRInputError_None) {
        continue;
      }
    } else if (handIndex == OpenVRHand::Right) {
      if (vr::VRInput()->GetInputSourceHandle(
              "/user/hand/right",
              &mControllerHand[OpenVRHand::Right].mSource) !=
          vr::VRInputError_None) {
        continue;
      }
    } else {
      MOZ_ASSERT(false, "Unknown OpenVR hand type.");
    }

    vr::InputOriginInfo_t originInfo;
    if (vr::VRInput()->GetOriginTrackedDeviceInfo(
            mControllerHand[handIndex].mSource, &originInfo,
            sizeof(originInfo)) == vr::VRInputError_None &&
        originInfo.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid &&
        mVRSystem->IsTrackedDeviceConnected(originInfo.trackedDeviceIndex)) {
      const ::vr::ETrackedDeviceClass deviceType =
          mVRSystem->GetTrackedDeviceClass(originInfo.trackedDeviceIndex);
      if (deviceType != ::vr::TrackedDeviceClass_Controller &&
          deviceType != ::vr::TrackedDeviceClass_GenericTracker) {
        continue;
      }

      if (mControllerDeviceIndex[stateIndex] != handIndex) {
        VRControllerState& controllerState = aState.controllerState[stateIndex];

        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionPose.name.BeginReading(),
            &mControllerHand[handIndex].mActionPose.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionHaptic.name.BeginReading(),
            &mControllerHand[handIndex].mActionHaptic.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionTrackpad_Analog.name.BeginReading(),
            &mControllerHand[handIndex].mActionTrackpad_Analog.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionTrackpad_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionTrackpad_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionTrackpad_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionTrackpad_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionTrigger_Value.name.BeginReading(),
            &mControllerHand[handIndex].mActionTrigger_Value.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionGrip_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionGrip_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionGrip_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionGrip_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionMenu_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionMenu_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionMenu_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionMenu_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionSystem_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionSystem_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionSystem_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionSystem_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionA_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionA_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionA_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionA_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionB_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionB_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex].mActionB_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionB_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionThumbstick_Analog.name.BeginReading(),
            &mControllerHand[handIndex].mActionThumbstick_Analog.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionThumbstick_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionThumbstick_Pressed.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionThumbstick_Touched.name.BeginReading(),
            &mControllerHand[handIndex].mActionThumbstick_Touched.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionFingerIndex_Value.name.BeginReading(),
            &mControllerHand[handIndex].mActionFingerIndex_Value.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionFingerMiddle_Value.name.BeginReading(),
            &mControllerHand[handIndex].mActionFingerMiddle_Value.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionFingerRing_Value.name.BeginReading(),
            &mControllerHand[handIndex].mActionFingerRing_Value.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionFingerPinky_Value.name.BeginReading(),
            &mControllerHand[handIndex].mActionFingerPinky_Value.handle);
        vr::VRInput()->GetActionHandle(
            mControllerHand[handIndex]
                .mActionBumper_Pressed.name.BeginReading(),
            &mControllerHand[handIndex].mActionBumper_Pressed.handle);

        nsCString deviceId;
        GetControllerDeviceId(deviceType, originInfo.trackedDeviceIndex,
                              deviceId);
        strncpy(controllerState.controllerName, deviceId.BeginReading(),
                kVRControllerNameMaxLen);
        controllerState.numHaptics = kNumOpenVRHaptics;
      }
      controllerPresent[stateIndex] = true;
      mControllerDeviceIndex[stateIndex] = static_cast<OpenVRHand>(handIndex);
      ++stateIndex;
    }
  }

  // Clear out entries for disconnected controllers
  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    if (!controllerPresent[stateIndex] &&
        mControllerDeviceIndex[stateIndex] != OpenVRHand::None) {
      mControllerDeviceIndex[stateIndex] = OpenVRHand::None;
      memset(&aState.controllerState[stateIndex], 0, sizeof(VRControllerState));
    }
  }
}

void OpenVRSession::EnumerateControllersObsolete(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  bool controllerPresent[kVRControllerMaxCount] = {false};

  // Basically, we would have HMDs in the tracked devices,
  // but we are just interested in the controllers.
  for (::vr::TrackedDeviceIndex_t trackedDevice =
           ::vr::k_unTrackedDeviceIndex_Hmd + 1;
       trackedDevice < ::vr::k_unMaxTrackedDeviceCount; ++trackedDevice) {
    if (!mVRSystem->IsTrackedDeviceConnected(trackedDevice)) {
      continue;
    }

    const ::vr::ETrackedDeviceClass deviceType =
        mVRSystem->GetTrackedDeviceClass(trackedDevice);
    if (deviceType != ::vr::TrackedDeviceClass_Controller &&
        deviceType != ::vr::TrackedDeviceClass_GenericTracker) {
      continue;
    }

    uint32_t stateIndex = 0;
    uint32_t firstEmptyIndex = kVRControllerMaxCount;

    // Find the existing controller
    for (stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
      if (mControllerDeviceIndexObsolete[stateIndex] == 0 &&
          firstEmptyIndex == kVRControllerMaxCount) {
        firstEmptyIndex = stateIndex;
      }
      if (mControllerDeviceIndexObsolete[stateIndex] == trackedDevice) {
        break;
      }
    }
    if (stateIndex == kVRControllerMaxCount) {
      // This is a new controller, let's add it
      if (firstEmptyIndex == kVRControllerMaxCount) {
        NS_WARNING(
            "OpenVR - Too many controllers, need to increase "
            "kVRControllerMaxCount.");
        continue;
      }
      stateIndex = firstEmptyIndex;
      mControllerDeviceIndexObsolete[stateIndex] = trackedDevice;
      VRControllerState& controllerState = aState.controllerState[stateIndex];
      uint32_t numButtons = 0;
      uint32_t numAxes = 0;

      // Scan the axes that the controllers support
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t supportAxis = mVRSystem->GetInt32TrackedDeviceProperty(
            trackedDevice, static_cast<vr::TrackedDeviceProperty>(
                               ::vr::Prop_Axis0Type_Int32 + j));
        switch (supportAxis) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad:
            numAxes += 2;  // It has x and y axes.
            ++numButtons;
            break;
          case ::vr::k_eControllerAxis_Trigger:
            if (j <= 2) {
              ++numButtons;
            } else {
#ifdef DEBUG
              // SteamVR Knuckles is the only special case for using 2D axis
              // values on triggers.
              ::vr::ETrackedPropertyError err;
              uint32_t requiredBufferLen;
              char charBuf[128];
              requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
                  trackedDevice, ::vr::Prop_RenderModelName_String, charBuf,
                  128, &err);
              MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
              nsCString deviceId(charBuf);
              MOZ_ASSERT(deviceId.Find("knuckles") != kNotFound);
#endif  // #ifdef DEBUG
              numButtons += 2;
            }
            break;
        }
      }

      // Scan the buttons that the controllers support
      const uint64_t supportButtons = mVRSystem->GetUint64TrackedDeviceProperty(
          trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_A)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_Grip)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        ++numButtons;
      }
      if (supportButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        ++numButtons;
      }

      nsCString deviceId;
      GetControllerDeviceId(deviceType, trackedDevice, deviceId);

      strncpy(controllerState.controllerName, deviceId.BeginReading(),
              kVRControllerNameMaxLen);
      controllerState.numButtons = numButtons;
      controllerState.numAxes = numAxes;
      controllerState.numHaptics = kNumOpenVRHaptics;

      // If the Windows MR controller doesn't has the amount
      // of buttons or axes as our expectation, switching off
      // the workaround for Windows MR.
      if (mIsWindowsMR && (numAxes < 4 || numButtons < 5)) {
        mIsWindowsMR = false;
        NS_WARNING("OpenVR - Switching off Windows MR mode.");
      }
    }
    controllerPresent[stateIndex] = true;
  }
  // Clear out entries for disconnected controllers
  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    if (!controllerPresent[stateIndex] &&
        mControllerDeviceIndexObsolete[stateIndex] != 0) {
      mControllerDeviceIndexObsolete[stateIndex] = 0;
      memset(&aState.controllerState[stateIndex], 0, sizeof(VRControllerState));
    }
  }
}

void OpenVRSession::UpdateControllerButtons(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis
  // value. In order to not affect the current VR content, we add a workaround
  // for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold =
      StaticPrefs::dom_vr_controller_trigger_threshold();

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       ++stateIndex) {
    OpenVRHand trackedDevice = mControllerDeviceIndex[stateIndex];
    if (trackedDevice == OpenVRHand::None) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    controllerState.hand = GetControllerHandFromControllerRole(trackedDevice);

    uint32_t axisIdx = 0;
    uint32_t buttonIdx = 0;
    // Axis 0 1: Trackpad
    // Button 0: Trackpad
    vr::InputAnalogActionData_t analogData;
    if (mControllerHand[stateIndex].mActionTrackpad_Analog.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionTrackpad_Analog.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      controllerState.axisValue[axisIdx] = analogData.x;
      ++axisIdx;
      controllerState.axisValue[axisIdx] = analogData.y * yAxisInvert;
      ++axisIdx;
    }
    vr::InputDigitalActionData_t actionData;
    bool bPressed = false;
    bool bTouched = false;
    uint64_t mask = 0;
    if (mControllerHand[stateIndex].mActionTrackpad_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionTrackpad_Pressed.handle,
            &actionData, sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionTrackpad_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionTrackpad_Touched.handle,
              &actionData, sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 1: Trigger
    if (mControllerHand[stateIndex].mActionTrigger_Value.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionTrigger_Value.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
      ++buttonIdx;
    }

    // Button 2: Grip
    if (mControllerHand[stateIndex].mActionGrip_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionGrip_Pressed.handle, &actionData,
            sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionGrip_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionGrip_Touched.handle,
              &actionData, sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 3: Menu
    if (mControllerHand[stateIndex].mActionMenu_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionMenu_Pressed.handle, &actionData,
            sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionMenu_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionMenu_Touched.handle,
              &actionData, sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 3: System
    if (mControllerHand[stateIndex].mActionSystem_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionSystem_Pressed.handle,
            &actionData, sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionSystem_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionSystem_Touched.handle,
              &actionData, sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 4: A
    if (mControllerHand[stateIndex].mActionA_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionA_Pressed.handle, &actionData,
            sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionA_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionA_Touched.handle, &actionData,
              sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 5: B
    if (mControllerHand[stateIndex].mActionB_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionB_Pressed.handle, &actionData,
            sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionB_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionB_Touched.handle, &actionData,
              sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Axis 2 3: Thumbstick
    // Button 6: Thumbstick
    if (mControllerHand[stateIndex].mActionThumbstick_Analog.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionThumbstick_Analog.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      controllerState.axisValue[axisIdx] = analogData.x;
      ++axisIdx;
      controllerState.axisValue[axisIdx] = analogData.y * yAxisInvert;
      ++axisIdx;
    }
    if (mControllerHand[stateIndex].mActionThumbstick_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionThumbstick_Pressed.handle,
            &actionData, sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      if (mControllerHand[stateIndex].mActionThumbstick_Touched.handle &&
          vr::VRInput()->GetDigitalActionData(
              mControllerHand[stateIndex].mActionThumbstick_Touched.handle,
              &actionData, sizeof(actionData),
              vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
        bTouched = actionData.bActive && actionData.bState;
        mask = (1ULL << buttonIdx);
        if (bTouched) {
          controllerState.buttonTouched |= mask;
        } else {
          controllerState.buttonTouched &= ~mask;
        }
      }
      ++buttonIdx;
    }

    // Button 7: Bumper (Cosmos only)
    if (mControllerHand[stateIndex].mActionBumper_Pressed.handle &&
        vr::VRInput()->GetDigitalActionData(
            mControllerHand[stateIndex].mActionBumper_Pressed.handle,
            &actionData, sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        actionData.bActive) {
      bPressed = actionData.bState;
      mask = (1ULL << buttonIdx);
      controllerState.triggerValue[buttonIdx] = bPressed ? 1.0 : 0.0f;
      if (bPressed) {
        controllerState.buttonPressed |= mask;
      } else {
        controllerState.buttonPressed &= ~mask;
      }
      ++buttonIdx;
    }

    // Button 7: Finger index
    if (mControllerHand[stateIndex].mActionFingerIndex_Value.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionFingerIndex_Value.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
      ++buttonIdx;
    }

    // Button 8: Finger middle
    if (mControllerHand[stateIndex].mActionFingerMiddle_Value.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionFingerMiddle_Value.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
      ++buttonIdx;
    }

    // Button 9: Finger ring
    if (mControllerHand[stateIndex].mActionFingerRing_Value.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionFingerRing_Value.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
      ++buttonIdx;
    }

    // Button 10: Finger pinky
    if (mControllerHand[stateIndex].mActionFingerPinky_Value.handle &&
        vr::VRInput()->GetAnalogActionData(
            mControllerHand[stateIndex].mActionFingerPinky_Value.handle,
            &analogData, sizeof(analogData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        analogData.bActive) {
      UpdateTrigger(controllerState, buttonIdx, analogData.x, triggerThreshold);
      ++buttonIdx;
    }

    controllerState.numButtons = buttonIdx;
    controllerState.numAxes = axisIdx;
  }
}

void OpenVRSession::UpdateControllerButtonsObsolete(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  // Compared to Edge, we have a wrong implementation for the vertical axis
  // value. In order to not affect the current VR content, we add a workaround
  // for yAxis.
  const float yAxisInvert = (mIsWindowsMR) ? -1.0f : 1.0f;
  const float triggerThreshold =
      StaticPrefs::dom_vr_controller_trigger_threshold();

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice =
        mControllerDeviceIndexObsolete[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::ETrackedControllerRole role =
        mVRSystem->GetControllerRoleForTrackedDeviceIndex(trackedDevice);
    dom::GamepadHand hand = GetControllerHandFromControllerRole(role);
    controllerState.hand = hand;

    ::vr::VRControllerState_t vrControllerState;
    if (mVRSystem->GetControllerState(trackedDevice, &vrControllerState,
                                      sizeof(vrControllerState))) {
      uint32_t axisIdx = 0;
      uint32_t buttonIdx = 0;
      for (uint32_t j = 0; j < ::vr::k_unControllerStateAxisCount; ++j) {
        const uint32_t axisType = mVRSystem->GetInt32TrackedDeviceProperty(
            trackedDevice, static_cast<::vr::TrackedDeviceProperty>(
                               ::vr::Prop_Axis0Type_Int32 + j));
        switch (axisType) {
          case ::vr::EVRControllerAxisType::k_eControllerAxis_Joystick:
          case ::vr::EVRControllerAxisType::k_eControllerAxis_TrackPad: {
            if (mIsWindowsMR) {
              // Adjust the input mapping for Windows MR which has
              // different order.
              axisIdx = (axisIdx == 0) ? 2 : 0;
              buttonIdx = (buttonIdx == 0) ? 4 : 0;
            }

            controllerState.axisValue[axisIdx] = vrControllerState.rAxis[j].x;
            ++axisIdx;
            controllerState.axisValue[axisIdx] =
                vrControllerState.rAxis[j].y * yAxisInvert;
            ++axisIdx;
            uint64_t buttonMask = ::vr::ButtonMaskFromId(
                static_cast<::vr::EVRButtonId>(::vr::k_EButton_Axis0 + j));

            UpdateButton(controllerState, vrControllerState, buttonIdx,
                         buttonMask);
            ++buttonIdx;

            if (mIsWindowsMR) {
              axisIdx = (axisIdx == 4) ? 2 : 4;
              buttonIdx = (buttonIdx == 5) ? 1 : 2;
            }
            break;
          }
          case vr::EVRControllerAxisType::k_eControllerAxis_Trigger: {
            if (j <= 2) {
              UpdateTrigger(controllerState, buttonIdx,
                            vrControllerState.rAxis[j].x, triggerThreshold);
              ++buttonIdx;
            } else {
              // For SteamVR Knuckles.
              UpdateTrigger(controllerState, buttonIdx,
                            vrControllerState.rAxis[j].x, triggerThreshold);
              ++buttonIdx;
              UpdateTrigger(controllerState, buttonIdx,
                            vrControllerState.rAxis[j].y, triggerThreshold);
              ++buttonIdx;
            }
            break;
          }
        }
      }

      const uint64_t supportedButtons =
          mVRSystem->GetUint64TrackedDeviceProperty(
              trackedDevice, ::vr::Prop_SupportedButtons_Uint64);
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_A)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_A));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_Grip)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_Grip));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_ApplicationMenu)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_ApplicationMenu));
        ++buttonIdx;
      }
      if (mIsWindowsMR) {
        // button 4 in Windows MR has already been assigned
        // to k_eControllerAxis_TrackPad.
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Left)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Left));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Up)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Up));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Right)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Right));
        ++buttonIdx;
      }
      if (supportedButtons & BTN_MASK_FROM_ID(k_EButton_DPad_Down)) {
        UpdateButton(controllerState, vrControllerState, buttonIdx,
                     BTN_MASK_FROM_ID(k_EButton_DPad_Down));
        ++buttonIdx;
      }
    }
  }
}

void OpenVRSession::UpdateControllerPoses(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  for (int8_t handIndex = 0; handIndex < OpenVRHand::Total; ++handIndex) {
    VRControllerState& controllerState = aState.controllerState[handIndex];
    vr::InputPoseActionData_t poseData;
    if (vr::VRInput()->GetPoseActionData(
            mControllerHand[handIndex].mActionPose.handle,
            vr::TrackingUniverseSeated, 0, &poseData, sizeof(poseData),
            vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None ||
        !poseData.bActive || !poseData.pose.bPoseIsValid) {
      controllerState.isOrientationValid = false;
      controllerState.isPositionValid = false;
    } else {
      const ::vr::TrackedDevicePose_t& pose = poseData.pose;
      if (pose.bDeviceIsConnected) {
        controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
                                 dom::GamepadCapabilityFlags::Cap_Position);
      } else {
        controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
      }
      if (pose.bPoseIsValid &&
          pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
        gfx::Matrix4x4 m;

        // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
        // because of its arrangement, we can copy the 12 elements in and
        // then transpose them to the right place.  We do this so we can
        // pull out a Quaternion.
        memcpy(&m.components, &pose.mDeviceToAbsoluteTracking,
               sizeof(pose.mDeviceToAbsoluteTracking));
        m.Transpose();

        gfx::Quaternion rot;
        rot.SetFromRotationMatrix(m);
        rot.Invert();

        controllerState.pose.orientation[0] = rot.x;
        controllerState.pose.orientation[1] = rot.y;
        controllerState.pose.orientation[2] = rot.z;
        controllerState.pose.orientation[3] = rot.w;
        controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
        controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
        controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
        controllerState.pose.angularAcceleration[0] = 0.0f;
        controllerState.pose.angularAcceleration[1] = 0.0f;
        controllerState.pose.angularAcceleration[2] = 0.0f;
        controllerState.isOrientationValid = true;

        controllerState.pose.position[0] = m._41;
        controllerState.pose.position[1] = m._42;
        controllerState.pose.position[2] = m._43;
        controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
        controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
        controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
        controllerState.pose.linearAcceleration[0] = 0.0f;
        controllerState.pose.linearAcceleration[1] = 0.0f;
        controllerState.pose.linearAcceleration[2] = 0.0f;
        controllerState.isPositionValid = true;
      }
    }
  }
}

void OpenVRSession::UpdateControllerPosesObsolete(VRSystemState& aState) {
  MOZ_ASSERT(mVRSystem);

  ::vr::TrackedDevicePose_t poses[::vr::k_unMaxTrackedDeviceCount];
  mVRSystem->GetDeviceToAbsoluteTrackingPose(::vr::TrackingUniverseSeated, 0.0f,
                                             poses,
                                             ::vr::k_unMaxTrackedDeviceCount);

  for (uint32_t stateIndex = 0; stateIndex < kVRControllerMaxCount;
       stateIndex++) {
    ::vr::TrackedDeviceIndex_t trackedDevice =
        mControllerDeviceIndexObsolete[stateIndex];
    if (trackedDevice == 0) {
      continue;
    }
    VRControllerState& controllerState = aState.controllerState[stateIndex];
    const ::vr::TrackedDevicePose_t& pose = poses[trackedDevice];

    if (pose.bDeviceIsConnected) {
      controllerState.flags = (dom::GamepadCapabilityFlags::Cap_Orientation |
                               dom::GamepadCapabilityFlags::Cap_Position);
    } else {
      controllerState.flags = dom::GamepadCapabilityFlags::Cap_None;
    }
    if (pose.bPoseIsValid &&
        pose.eTrackingResult == ::vr::TrackingResult_Running_OK) {
      gfx::Matrix4x4 m;

      // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
      // because of its arrangement, we can copy the 12 elements in and
      // then transpose them to the right place.  We do this so we can
      // pull out a Quaternion.
      memcpy(&m.components, &pose.mDeviceToAbsoluteTracking,
             sizeof(pose.mDeviceToAbsoluteTracking));
      m.Transpose();

      gfx::Quaternion rot;
      rot.SetFromRotationMatrix(m);
      rot.Invert();

      controllerState.pose.orientation[0] = rot.x;
      controllerState.pose.orientation[1] = rot.y;
      controllerState.pose.orientation[2] = rot.z;
      controllerState.pose.orientation[3] = rot.w;
      controllerState.pose.angularVelocity[0] = pose.vAngularVelocity.v[0];
      controllerState.pose.angularVelocity[1] = pose.vAngularVelocity.v[1];
      controllerState.pose.angularVelocity[2] = pose.vAngularVelocity.v[2];
      controllerState.pose.angularAcceleration[0] = 0.0f;
      controllerState.pose.angularAcceleration[1] = 0.0f;
      controllerState.pose.angularAcceleration[2] = 0.0f;
      controllerState.isOrientationValid = true;

      controllerState.pose.position[0] = m._41;
      controllerState.pose.position[1] = m._42;
      controllerState.pose.position[2] = m._43;
      controllerState.pose.linearVelocity[0] = pose.vVelocity.v[0];
      controllerState.pose.linearVelocity[1] = pose.vVelocity.v[1];
      controllerState.pose.linearVelocity[2] = pose.vVelocity.v[2];
      controllerState.pose.linearAcceleration[0] = 0.0f;
      controllerState.pose.linearAcceleration[1] = 0.0f;
      controllerState.pose.linearAcceleration[2] = 0.0f;
      controllerState.isPositionValid = true;
    } else {
      controllerState.isOrientationValid = false;
      controllerState.isPositionValid = false;
    }
  }
}

void OpenVRSession::GetControllerDeviceId(
    ::vr::ETrackedDeviceClass aDeviceType,
    ::vr::TrackedDeviceIndex_t aDeviceIndex, nsCString& aId) {
  switch (aDeviceType) {
    case ::vr::TrackedDeviceClass_Controller: {
      ::vr::ETrackedPropertyError err;
      uint32_t requiredBufferLen;
      bool isFound = false;
      char charBuf[128];
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
          aDeviceIndex, ::vr::Prop_RenderModelName_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      nsCString deviceId(charBuf);
      if (deviceId.Find("knuckles") != kNotFound) {
        aId.AssignLiteral("OpenVR Knuckles");
        isFound = true;
      } else if (deviceId.Find("vive_cosmos_controller") != kNotFound) {
        aId.AssignLiteral("OpenVR Cosmos");
        isFound = true;
      }
      requiredBufferLen = mVRSystem->GetStringTrackedDeviceProperty(
          aDeviceIndex, ::vr::Prop_SerialNumber_String, charBuf, 128, &err);
      if (requiredBufferLen > 128) {
        MOZ_CRASH("Larger than the buffer size.");
      }
      MOZ_ASSERT(requiredBufferLen && err == ::vr::TrackedProp_Success);
      deviceId.Assign(charBuf);
      if (deviceId.Find("MRSOURCE") != kNotFound) {
        aId.AssignLiteral("Spatial Controller (Spatial Interaction Source) ");
        mIsWindowsMR = true;
        isFound = true;
      }
      if (!isFound) {
        aId.AssignLiteral("OpenVR Gamepad");
      }
      break;
    }
    case ::vr::TrackedDeviceClass_GenericTracker: {
      aId.AssignLiteral("OpenVR Tracker");
      break;
    }
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void OpenVRSession::StartFrame(mozilla::gfx::VRSystemState& aSystemState) {
  UpdateHeadsetPose(aSystemState);
  UpdateEyeParameters(aSystemState);

  if (StaticPrefs::dom_vr_openvr_action_input_AtStartup()) {
    EnumerateControllers(aSystemState);

    vr::VRActiveActionSet_t actionSet = {0};
    actionSet.ulActionSet = mActionsetFirefox;
    vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);

    UpdateControllerButtons(aSystemState);
    UpdateControllerPoses(aSystemState);
  } else {
    EnumerateControllersObsolete(aSystemState);
    UpdateControllerButtonsObsolete(aSystemState);
    UpdateControllerPosesObsolete(aSystemState);
  }

  UpdateTelemetry(aSystemState);
}

void OpenVRSession::ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) {
  bool isHmdPresent = ::vr::VR_IsHmdPresent();
  if (!isHmdPresent) {
    mShouldQuit = true;
  }

  ::vr::VREvent_t event;
  while (mVRSystem && mVRSystem->PollNextEvent(&event, sizeof(event))) {
    switch (event.eventType) {
      case ::vr::VREvent_TrackedDeviceUserInteractionStarted:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.isMounted = true;
        }
        break;
      case ::vr::VREvent_TrackedDeviceUserInteractionEnded:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.isMounted = false;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceActivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.isConnected = true;
        }
        break;
      case ::vr::EVREventType::VREvent_TrackedDeviceDeactivated:
        if (event.trackedDeviceIndex == ::vr::k_unTrackedDeviceIndex_Hmd) {
          aSystemState.displayState.isConnected = false;
        }
        break;
      case ::vr::EVREventType::VREvent_DriverRequestedQuit:
      case ::vr::EVREventType::VREvent_Quit:
      // When SteamVR runtime haven't been launched before viewing VR,
      // SteamVR will send a VREvent_ProcessQuit event. It will tell the parent
      // process to shutdown the VR process, and we need to avoid it.
      // case ::vr::EVREventType::VREvent_ProcessQuit:
      case ::vr::EVREventType::VREvent_QuitAcknowledged:
      case ::vr::EVREventType::VREvent_QuitAborted_UserPrompt:
        mShouldQuit = true;
        break;
      default:
        // ignore
        break;
    }
  }
}

#if defined(XP_WIN)
bool OpenVRSession::SubmitFrame(
    const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
    ID3D11Texture2D* aTexture) {
  return SubmitFrame((void*)aTexture, ::vr::ETextureType::TextureType_DirectX,
                     aLayer.leftEyeRect, aLayer.rightEyeRect);
}
#elif defined(XP_MACOSX)
bool OpenVRSession::SubmitFrame(
    const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
    const VRLayerTextureHandle& aTexture) {
  return SubmitFrame(aTexture, ::vr::ETextureType::TextureType_IOSurface,
                     aLayer.leftEyeRect, aLayer.rightEyeRect);
}
#endif

bool OpenVRSession::SubmitFrame(const VRLayerTextureHandle& aTextureHandle,
                                ::vr::ETextureType aTextureType,
                                const VRLayerEyeRect& aLeftEyeRect,
                                const VRLayerEyeRect& aRightEyeRect) {
  ::vr::Texture_t tex;
#if defined(XP_MACOSX)
  // We get aTextureHandle from get_SurfaceDescriptorMacIOSurface() at
  // VRDisplayExternal. scaleFactor and opaque are skipped because they always
  // are 1.0 and false.
  RefPtr<MacIOSurface> surf = MacIOSurface::LookupSurface(aTextureHandle);
  if (!surf) {
    NS_WARNING("OpenVRSession::SubmitFrame failed to get a MacIOSurface");
    return false;
  }

  CFTypeRefPtr<IOSurfaceRef> ioSurface = surf->GetIOSurfaceRef();
  tex.handle = (void*)ioSurface.get();
#else
  tex.handle = aTextureHandle;
#endif
  tex.eType = aTextureType;
  tex.eColorSpace = ::vr::EColorSpace::ColorSpace_Auto;

  ::vr::VRTextureBounds_t bounds;
  bounds.uMin = aLeftEyeRect.x;
  bounds.vMin = 1.0 - aLeftEyeRect.y;
  bounds.uMax = aLeftEyeRect.x + aLeftEyeRect.width;
  bounds.vMax = 1.0 - (aLeftEyeRect.y + aLeftEyeRect.height);

  ::vr::EVRCompositorError err;
  err = mVRCompositor->Submit(::vr::EVREye::Eye_Left, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  bounds.uMin = aRightEyeRect.x;
  bounds.vMin = 1.0 - aRightEyeRect.y;
  bounds.uMax = aRightEyeRect.x + aRightEyeRect.width;
  bounds.vMax = 1.0 - (aRightEyeRect.y + aRightEyeRect.height);

  err = mVRCompositor->Submit(::vr::EVREye::Eye_Right, &tex, &bounds);
  if (err != ::vr::EVRCompositorError::VRCompositorError_None) {
    printf_stderr("OpenVR Compositor Submit() failed.\n");
  }

  mVRCompositor->PostPresentHandoff();
  return true;
}

void OpenVRSession::StopPresentation() {
  mVRCompositor->ClearLastSubmittedFrame();

  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats,
                                    sizeof(::vr::Compositor_CumulativeStats));
}

bool OpenVRSession::StartPresentation() { return true; }

void OpenVRSession::VibrateHaptic(uint32_t aControllerIdx,
                                  uint32_t aHapticIndex, float aIntensity,
                                  float aDuration) {
  MutexAutoLock lock(mControllerHapticStateMutex);

  // Initilize the haptic thread when the first time to do vibration.
  if (!mHapticThread) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "OpenVRSession::StartHapticThread", [this]() { StartHapticThread(); }));
  }

  if (aHapticIndex >= kNumOpenVRHaptics ||
      aControllerIdx >= kVRControllerMaxCount) {
    return;
  }

  OpenVRHand deviceIndex = mControllerDeviceIndex[aControllerIdx];
  if (deviceIndex == OpenVRHand::None) {
    return;
  }

  mHapticPulseRemaining[aControllerIdx][aHapticIndex] = aDuration;
  mHapticPulseIntensity[aControllerIdx][aHapticIndex] = aIntensity;

  /**
   *  TODO - The haptic feedback pulses will have latency of one frame and we
   *         are simulating intensity with pulse-width modulation.
   *         We should use of the OpenVR Input API to correct this
   *         and replace the TriggerHapticPulse calls which have been
   *         deprecated.
   */
}

void OpenVRSession::StartHapticThread() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mHapticThread) {
    mHapticThread = new VRThread(NS_LITERAL_CSTRING("VR_OpenVR_Haptics"));
  }
  mHapticThread->Start();
  StartHapticTimer();
}

void OpenVRSession::StopHapticThread() {
  if (mHapticThread) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "mHapticThread::Shutdown",
        [thread = mHapticThread]() { thread->Shutdown(); }));
    mHapticThread = nullptr;
  }
}

void OpenVRSession::StartHapticTimer() {
  if (!mHapticTimer && mHapticThread) {
    mLastHapticUpdate = TimeStamp();
    mHapticTimer = NS_NewTimer();
    mHapticTimer->SetTarget(mHapticThread->GetThread()->EventTarget());
    mHapticTimer->InitWithNamedFuncCallback(
        HapticTimerCallback, this, kVRHapticUpdateInterval,
        nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
        "OpenVRSession::HapticTimerCallback");
  }
}

void OpenVRSession::StopHapticTimer() {
  if (mHapticTimer) {
    mHapticTimer->Cancel();
    mHapticTimer = nullptr;
  }
}

/*static*/
void OpenVRSession::HapticTimerCallback(nsITimer* aTimer, void* aClosure) {
  /**
   * It is safe to use the pointer passed in aClosure to reference the
   * OpenVRSession object as the timer is canceled in OpenVRSession::Shutdown,
   * which is called by the OpenVRSession destructor, guaranteeing
   * that this function runs if and only if the VRManager object is valid.
   */
  OpenVRSession* self = static_cast<OpenVRSession*>(aClosure);

  if (StaticPrefs::dom_vr_openvr_action_input_AtStartup()) {
    self->UpdateHaptics();
  } else {
    self->UpdateHapticsObsolete();
  }
}

void OpenVRSession::UpdateHaptics() {
  MOZ_ASSERT(mHapticThread->GetThread() == NS_GetCurrentThread());
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  TimeStamp now = TimeStamp::Now();
  if (mLastHapticUpdate.IsNull()) {
    mLastHapticUpdate = now;
    return;
  }
  float deltaTime = (float)(now - mLastHapticUpdate).ToSeconds();
  mLastHapticUpdate = now;

  for (int iController = 0; iController < kVRControllerMaxCount;
       iController++) {
    for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
      OpenVRHand deviceIndex = mControllerDeviceIndex[iController];
      if (deviceIndex == OpenVRHand::None) {
        continue;
      }
      float intensity = mHapticPulseIntensity[iController][iHaptic];
      float duration = mHapticPulseRemaining[iController][iHaptic];
      if (duration <= 0.0f || intensity <= 0.0f) {
        continue;
      }
      vr::VRInput()->TriggerHapticVibrationAction(
          mControllerHand[iController].mActionHaptic.handle, 0.0f, deltaTime,
          4.0f, intensity > 1.0 ? 1.0 : intensity,
          vr::k_ulInvalidInputValueHandle);

      duration -= deltaTime;
      if (duration < 0.0f) {
        duration = 0.0f;
      }
      mHapticPulseRemaining[iController][iHaptic] = duration;
    }
  }
}

void OpenVRSession::UpdateHapticsObsolete() {
  MOZ_ASSERT(mHapticThread->GetThread() == NS_GetCurrentThread());
  MOZ_ASSERT(mVRSystem);

  MutexAutoLock lock(mControllerHapticStateMutex);

  TimeStamp now = TimeStamp::Now();
  if (mLastHapticUpdate.IsNull()) {
    mLastHapticUpdate = now;
    return;
  }
  float deltaTime = (float)(now - mLastHapticUpdate).ToSeconds();
  mLastHapticUpdate = now;

  for (int iController = 0; iController < kVRControllerMaxCount;
       iController++) {
    for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
      ::vr::TrackedDeviceIndex_t deviceIndex =
          mControllerDeviceIndexObsolete[iController];
      if (deviceIndex == 0) {
        continue;
      }
      float intensity = mHapticPulseIntensity[iController][iHaptic];
      float duration = mHapticPulseRemaining[iController][iHaptic];
      if (duration <= 0.0f || intensity <= 0.0f) {
        continue;
      }
      // We expect OpenVR to vibrate for 5 ms, but we found it only response the
      // commend ~ 3.9 ms. For duration time longer than 3.9 ms, we separate
      // them to a loop of 3.9 ms for make users feel that is a continuous
      // events.
      const float microSec =
          (duration < 0.0039f ? duration : 0.0039f) * 1000000.0f * intensity;
      mVRSystem->TriggerHapticPulse(deviceIndex, iHaptic, (uint32_t)microSec);

      duration -= deltaTime;
      if (duration < 0.0f) {
        duration = 0.0f;
      }
      mHapticPulseRemaining[iController][iHaptic] = duration;
    }
  }
}

void OpenVRSession::StopVibrateHaptic(uint32_t aControllerIdx) {
  MutexAutoLock lock(mControllerHapticStateMutex);
  if (aControllerIdx >= kVRControllerMaxCount) {
    return;
  }
  for (int iHaptic = 0; iHaptic < kNumOpenVRHaptics; iHaptic++) {
    mHapticPulseRemaining[aControllerIdx][iHaptic] = 0.0f;
  }
}

void OpenVRSession::StopAllHaptics() {
  MutexAutoLock lock(mControllerHapticStateMutex);
  for (auto& controller : mHapticPulseRemaining) {
    for (auto& haptic : controller) {
      haptic = 0.0f;
    }
  }
}

void OpenVRSession::UpdateTelemetry(VRSystemState& aSystemState) {
  ::vr::Compositor_CumulativeStats stats;
  mVRCompositor->GetCumulativeStats(&stats,
                                    sizeof(::vr::Compositor_CumulativeStats));
  aSystemState.displayState.droppedFrameCount = stats.m_nNumReprojectedFrames;
}

}  // namespace gfx
}  // namespace mozilla
