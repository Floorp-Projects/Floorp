/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLContextTypes.h"
#include "GLImages.h"
#include "GLLibraryEGL.h"

#include "gfxPrefs.h"
#include "gfxVRGVRAPI.h"
#include "gfxVRGVR.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Quaternion.h"
#include "mozilla/jni/Utils.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/Preferences.h"

#include "GeckoVRManager.h"
#include "nsString.h"

#include "SurfaceTypes.h"

#include "VRManager.h"

#define MOZ_CHECK_GVR_ERRORS

#if defined(MOZ_CHECK_GVR_ERRORS)
#define GVR_LOGTAG "GeckoWebVR"
#include <android/log.h>
#define GVR_CHECK(X) X; \
{ \
  gvr_context* context = (mPresentingContext ? mPresentingContext : GetNonPresentingContext()); \
  if (context && (gvr_get_error(context) != GVR_ERROR_NONE)) { \
     __android_log_print(ANDROID_LOG_ERROR, GVR_LOGTAG, \
                         "GVR ERROR: %s at%s:%s:%d", \
                         gvr_get_error_string(gvr_get_error(context)), \
                         __FILE__, __FUNCTION__, __LINE__); \
    gvr_clear_error(context); \
  } else if (!context) { \
    __android_log_print(ANDROID_LOG_ERROR, GVR_LOGTAG, \
                        "UNABLE TO CHECK GVR ERROR: NO CONTEXT"); \
  } \
}
#define GVR_LOG(format, ...) __android_log_print(ANDROID_LOG_INFO, GVR_LOGTAG, format, ##__VA_ARGS__);
#else
#define GVR_CHECK(X) X
#define GVR_LOG(...)
#endif

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;
using namespace mozilla::dom;

namespace {
static VRDisplayGVR* sContextObserver;
static RefPtr<GLContextEGL> sGLContextEGL;
static gvr_context* sNonPresentingContext;

gvr_context*
GetNonPresentingContext() {
  if (!sNonPresentingContext) {
    // Try and restore if it has been lost
    sNonPresentingContext = (gvr_context*)GeckoVRManager::CreateGVRNonPresentingContext();
  }
  return sNonPresentingContext;
}

class SynchronousRunnable : public nsIRunnable {
public:
  enum class Type {
    PresentingContext,
    NonPresentingContext,
    Pause,
    Resume
  };
  SynchronousRunnable(const Type aType, void* aContext)
  : mType(aType)
  , mContext(aContext)
  , mUpdateMonitor(new Monitor("SynchronousRunnable_for_Android"))
  , mUpdated(false)
  {}
  NS_DECL_THREADSAFE_ISUPPORTS
  nsresult Run() override
  {
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    MonitorAutoLock lock(*mUpdateMonitor);
    if (mType == Type::PresentingContext) {
      SetGVRPresentingContext(mContext);
    } else if (mType == Type::NonPresentingContext) {
      CleanupGVRNonPresentingContext();
    } else if (mType == Type::Pause) {
      SetGVRPaused(true);
    } else if (mType == Type::Resume) {
      SetGVRPaused(false);
    } else {
      GVR_LOG("UNKNOWN SynchronousRunnable::Type!");
    }
    mUpdated = true;
    lock.NotifyAll();
    return NS_OK;
  }
  void Wait()
  {
    MonitorAutoLock lock(*mUpdateMonitor);
    while(!mUpdated) {
      lock.Wait();
    }
  }

  static bool Dispatch(const Type aType, void* aContext)
  {
    if (!CompositorThreadHolder::IsInCompositorThread()) {
      RefPtr<SynchronousRunnable> runnable = new SynchronousRunnable(aType, aContext);
      CompositorThreadHolder::Loop()->PostTask(do_AddRef(runnable));
      runnable->Wait();
      return true;
    }

    return false;
  }

protected:
  virtual ~SynchronousRunnable()
  {
    delete mUpdateMonitor;
  }

  Type mType;
  void* mContext;
  Monitor* mUpdateMonitor;
  bool mUpdated;
};

}

NS_IMPL_ISUPPORTS(SynchronousRunnable, nsIRunnable)

void
mozilla::gfx::SetGVRPresentingContext(void* aGVRPresentingContext)
{
  if (SynchronousRunnable::Dispatch(SynchronousRunnable::Type::PresentingContext, aGVRPresentingContext)) {
    GVR_LOG("Done waiting for compositor thread to set presenting context.");
    return;
  }

  MOZ_ASSERT(sContextObserver);
  if (!sGLContextEGL && aGVRPresentingContext) {
    CreateContextFlags flags = CreateContextFlags::NONE;
    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    nsCString str;
    sGLContextEGL = GLContextEGL::CreateEGLPBufferOffscreenContext(flags, IntSize(4, 4), caps, &str);
    if (!sGLContextEGL->MakeCurrent()) {
      GVR_LOG("Failed to make GL context current");
    }
  }
  sContextObserver->SetPresentingContext(aGVRPresentingContext);
}

void
mozilla::gfx::CleanupGVRNonPresentingContext()
{
  if (SynchronousRunnable::Dispatch(SynchronousRunnable::Type::NonPresentingContext, nullptr)) {
    GVR_LOG("Done waiting for compositor thread to set non presenting context.");
    return;
  }

  if (sNonPresentingContext) {
    sNonPresentingContext = nullptr;
    GeckoVRManager::DestroyGVRNonPresentingContext();
  }
}

void
mozilla::gfx::SetGVRPaused(const bool aPaused)
{
  if (SynchronousRunnable::Dispatch((aPaused ? SynchronousRunnable::Type::Pause : SynchronousRunnable::Type::Resume), nullptr)) {
    GVR_LOG("Done waiting for GVR in compositor to: %s",(aPaused ? "Pause" : "Resume"));
    return;
  }
  MOZ_ASSERT(sContextObserver);
  sContextObserver->SetPaused(aPaused);
}

VRDisplayGVR::VRDisplayGVR()
  : VRDisplayHost(VRDeviceType::GVR)
  , mIsPresenting(false)
  , mControllerAdded(false)
  , mPresentingContext(nullptr)
  , mControllerContext(nullptr)
  , mControllerState(nullptr)
  , mViewportList(nullptr)
  , mLeftViewport(nullptr)
  , mRightViewport(nullptr)
  , mSwapChain(nullptr)
  , mFrameBufferSize{0, 0}
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayGVR, VRDisplayHost);
  MOZ_ASSERT(GetNonPresentingContext());
  MOZ_ASSERT(!sContextObserver); // There can be only one GVR display at a time.
  sContextObserver = this;

  mDisplayInfo.mDisplayName.AssignLiteral("GVR HMD");
  mDisplayInfo.mIsConnected = true;
  mDisplayInfo.mIsMounted = true;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_Position | // Not yet...
                                  VRDisplayCapabilityFlags::Cap_Present;

  GVR_CHECK(gvr_refresh_viewer_profile(GetNonPresentingContext()));
  mViewportList = GVR_CHECK(gvr_buffer_viewport_list_create(GetNonPresentingContext()));
  mLeftViewport = GVR_CHECK(gvr_buffer_viewport_create(GetNonPresentingContext()));
  mRightViewport = GVR_CHECK(gvr_buffer_viewport_create(GetNonPresentingContext()));
  UpdateViewport();

  dom::GamepadHand hand = dom::GamepadHand::Right;
  const gvr_user_prefs* prefs = GVR_CHECK(gvr_get_user_prefs(GetNonPresentingContext()));
  if (prefs) {
    hand = ((gvr_user_prefs_get_controller_handedness(prefs) == GVR_CONTROLLER_RIGHT_HANDED) ?
             dom::GamepadHand::Right : dom::GamepadHand::Left);
  }
  mController = new VRControllerGVR(hand, mDisplayInfo.mDisplayID);
}

VRDisplayGVR::~VRDisplayGVR()
{
  MOZ_COUNT_DTOR_INHERITED(VRDisplayGVR, VRDisplayHost);
}

void
VRDisplayGVR::ZeroSensor()
{
}

void
VRDisplayGVR::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }

  mIsPresenting = true;
  GeckoVRManager::EnableVRMode();
}

void
VRDisplayGVR::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }

  mIsPresenting = false;
  GeckoVRManager::DisableVRMode();
}

bool
VRDisplayGVR::SubmitFrame(const mozilla::layers::EGLImageDescriptor* aDescriptor,
                          const gfx::Rect& aLeftEyeRect,
                          const gfx::Rect& aRightEyeRect)
{
  if (!mPresentingContext) {
    GVR_LOG("Unable to submit frame. No presenting context")
    return false;
  }
  if (!sGLContextEGL) {
    GVR_LOG("Unable to submit frame. No GL Context");
    return false;
  }

  if (!sGLContextEGL->MakeCurrent()) {
    GVR_LOG("Failed to make GL context current");
    return false;
  }

  EGLImage image = (EGLImage)aDescriptor->image();
  EGLSync sync = (EGLSync)aDescriptor->fence();
  gfx::IntSize size = aDescriptor->size();
  MOZ_ASSERT(mSwapChain);
  GVR_CHECK(gvr_get_recommended_buffer_viewports(mPresentingContext, mViewportList));
  if ((size.width != mFrameBufferSize.width) || (size.height != mFrameBufferSize.height)) {
    mFrameBufferSize.width = size.width;
    mFrameBufferSize.height = size.height;
    GVR_CHECK(gvr_swap_chain_resize_buffer(mSwapChain, 0, mFrameBufferSize));
    GVR_LOG("Resize Swap Chain %d,%d", mFrameBufferSize.width, mFrameBufferSize.height);
  }
  gvr_frame* frame = GVR_CHECK(gvr_swap_chain_acquire_frame(mSwapChain));
  if (!frame) {
    // Sometimes the swap chain seems to not initialized correctly so that
    // frames can not be acquired. Recreating the swap chain seems to fix the
    // issue.
    GVR_LOG("Unable to acquire GVR frame. Recreating swap chain.");
    RecreateSwapChain();
    return false;
  }
  GVR_CHECK(gvr_frame_bind_buffer(frame, 0));

  EGLint status = LOCAL_EGL_CONDITION_SATISFIED;

  if (sync) {
    MOZ_ASSERT(sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));
    status = sEGLLibrary.fClientWaitSync(EGL_DISPLAY(), sync, 0, LOCAL_EGL_FOREVER);
  }

  if (status != LOCAL_EGL_CONDITION_SATISFIED) {
    MOZ_ASSERT(status != 0,
               "ClientWaitSync generated an error. Has sync already been destroyed?");
    return false;
  }

  if (image) {
    GLuint tex = 0;
    sGLContextEGL->fGenTextures(1, &tex);

    const ScopedSaveMultiTex saveTex(sGLContextEGL, 1, LOCAL_GL_TEXTURE_2D);
    sGLContextEGL->fBindTexture(LOCAL_GL_TEXTURE_2D, tex);
    sGLContextEGL->TexParams_SetClampNoMips();
    sGLContextEGL->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, image);
    sGLContextEGL->BlitHelper()->DrawBlitTextureToFramebuffer(tex, gfx::IntSize(size.width, size.height), gfx::IntSize(mFrameBufferSize.width,  mFrameBufferSize.height));
    sGLContextEGL->fDeleteTextures(1, &tex);
  } else {
    GVR_LOG("Unable to submit frame. Unable to extract EGLImage");
    return false;
  }
  GVR_CHECK(gvr_frame_unbind(frame));
  GVR_CHECK(gvr_frame_submit(&frame, mViewportList, mHeadMatrix));
  return true;
}

static void
FillMatrix(gfx::Matrix4x4 &target, const gvr_mat4f& source)
{
  target._11 = source.m[0][0];
  target._12 = source.m[0][1];
  target._13 = source.m[0][2];
  target._14 = source.m[0][3];
  target._21 = source.m[1][0];
  target._22 = source.m[1][1];
  target._23 = source.m[1][2];
  target._24 = source.m[1][3];
  target._31 = source.m[2][0];
  target._32 = source.m[2][1];
  target._33 = source.m[2][2];
  target._34 = source.m[2][3];
  target._41 = source.m[3][0];
  target._42 = source.m[3][1];
  target._43 = source.m[3][2];
  target._44 = source.m[3][3];
}

VRHMDSensorState
VRDisplayGVR::GetSensorState()
{
  VRHMDSensorState result{};

  gvr_context* context = (mPresentingContext ? mPresentingContext : GetNonPresentingContext());

  if (!context) {
    GVR_LOG("Unable to get sensor state. Context is null");
    return result;
  }

  gvr_clock_time_point when = GVR_CHECK(gvr_get_time_point_now());
  if (mIsPresenting) {
    // 50ms into the future is what GVR docs recommends using for head rotation
    // prediction.
    when.monotonic_system_time_nanos += 50000000;
  }
  mHeadMatrix = GVR_CHECK(gvr_get_head_space_from_start_space_rotation(context, when));
  gvr_mat4f neck = GVR_CHECK(gvr_apply_neck_model(context, mHeadMatrix, 1.0));;

  gfx::Matrix4x4 m;

  FillMatrix(m, neck);
  m.Invert();
  gfx::Quaternion rot;
  rot.SetFromRotationMatrix(m);

  result.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
  result.orientation[0] = rot.x;
  result.orientation[1] = rot.y;
  result.orientation[2] = rot.z;
  result.orientation[3] = rot.w;
  result.angularVelocity[0] = 0.0f;
  result.angularVelocity[1] = 0.0f;
  result.angularVelocity[2] = 0.0f;

  result.flags |= VRDisplayCapabilityFlags::Cap_Position;
  result.position[0] = m._14;
  result.position[1] = m._24;
  result.position[2] = m._34;
  result.linearVelocity[0] = 0.0f;
  result.linearVelocity[1] = 0.0f;
  result.linearVelocity[2] = 0.0f;

  UpdateHeadToEye(context, &rot);
  result.CalcViewMatrices(mHeadToEyes);

  return result;
}

void
VRDisplayGVR::SetPaused(const bool aPaused)
{
  if (aPaused) {
    if (mPresentingContext) {
      GVR_CHECK(gvr_pause_tracking(mPresentingContext));
    } else if (sNonPresentingContext) {
      GVR_CHECK(gvr_pause_tracking(sNonPresentingContext));
    }

    if (mControllerContext) {
      GVR_CHECK(gvr_controller_pause(mControllerContext));
    }
  } else {
    if (mPresentingContext) {
      GVR_CHECK(gvr_refresh_viewer_profile(mPresentingContext));
      GVR_CHECK(gvr_resume_tracking(mPresentingContext));
    } else if (sNonPresentingContext) {
      GVR_CHECK(gvr_resume_tracking(sNonPresentingContext));
    }

    if (mControllerContext) {
      GVR_CHECK(gvr_controller_resume(mControllerContext));
    }
  }
}

void
VRDisplayGVR::SetPresentingContext(void* aGVRPresentingContext)
{
  MOZ_ASSERT(sGLContextEGL);
  sGLContextEGL->MakeCurrent();
  mPresentingContext = (gvr_context*)aGVRPresentingContext;
  if (mPresentingContext) {
    GVR_CHECK(gvr_initialize_gl(mPresentingContext));
    RecreateSwapChain();
  } else {

    if (mSwapChain) {
      // gvr_swap_chain_destroy will set the pointer to null
      GVR_CHECK(gvr_swap_chain_destroy(&mSwapChain));
      MOZ_ASSERT(!mSwapChain);
    }

    // The presentation context has been destroy, probably by the user so increment the presenting
    // generation if we are presenting so that the DOM knows to end the current presentation.
    if (mIsPresenting) {
      mDisplayInfo.mPresentingGeneration++;
    }
  }
}

void
VRDisplayGVR::UpdateHeadToEye(gvr_context* aContext, gfx::Quaternion* aRot)
{
  if (!aContext) {
    return;
  }

  for (uint32_t eyeIndex = 0; eyeIndex < 2; eyeIndex++) {
    gvr_mat4f eye = GVR_CHECK(gvr_get_eye_from_head_matrix(aContext, eyeIndex));
    mDisplayInfo.mEyeTranslation[eyeIndex].x = -eye.m[0][3];
    mDisplayInfo.mEyeTranslation[eyeIndex].y = -eye.m[1][3];
    mDisplayInfo.mEyeTranslation[eyeIndex].z = -eye.m[2][3];
    if (aRot) {
      mHeadToEyes[eyeIndex].SetRotationFromQuaternion(*aRot);
    } else {
      mHeadToEyes[eyeIndex] = gfx::Matrix4x4();
    }
    mHeadToEyes[eyeIndex].PreTranslate(eye.m[0][3], eye.m[1][3], eye.m[2][3]);
  }
}

void
VRDisplayGVR::UpdateViewport()
{
  gvr_context* context = (mPresentingContext ? mPresentingContext : GetNonPresentingContext());

  if (!context) {
    return;
  }

  GVR_CHECK(gvr_get_recommended_buffer_viewports(context, mViewportList));
  GVR_CHECK(gvr_buffer_viewport_list_get_item(mViewportList, 0, mLeftViewport));
  GVR_CHECK(gvr_buffer_viewport_list_get_item(mViewportList, 1, mRightViewport));

  gvr_rectf fov = GVR_CHECK(gvr_buffer_viewport_get_source_fov(mLeftViewport));
  mDisplayInfo.mEyeFOV[VRDisplayInfo::Eye_Left] = VRFieldOfView(fov.top, fov.right, fov.bottom, fov.left);
  GVR_LOG("FOV:L top:%f right:%f bottom:%f left:%f",(float)fov.top, (float)fov.left, (float)fov.bottom, (float)fov.right);

  fov = GVR_CHECK(gvr_buffer_viewport_get_source_fov(mRightViewport));
  mDisplayInfo.mEyeFOV[VRDisplayInfo::Eye_Right] = VRFieldOfView(fov.top, fov.right, fov.bottom, fov.left);
  GVR_LOG("FOV:R top:%f right:%f bottom:%f left:%f",(float)fov.top, (float)fov.left, (float)fov.bottom, (float)fov.right);

  gvr_sizei size = GVR_CHECK(gvr_get_maximum_effective_render_target_size(context));
  mDisplayInfo.mEyeResolution = IntSize(size.width / 2, size.height);
  GVR_LOG("Eye Resolution: %dx%d",mDisplayInfo.mEyeResolution.width,mDisplayInfo.mEyeResolution.height);

  UpdateHeadToEye(context);
}

void
VRDisplayGVR::RecreateSwapChain()
{
  MOZ_ASSERT(sGLContextEGL);
  sGLContextEGL->MakeCurrent();
  if (mSwapChain) {
    // gvr_swap_chain_destroy will set the pointer to null
    GVR_CHECK(gvr_swap_chain_destroy(&mSwapChain));
    MOZ_ASSERT(!mSwapChain);
  }
  gvr_buffer_spec* spec = GVR_CHECK(gvr_buffer_spec_create(mPresentingContext));
  mFrameBufferSize = GVR_CHECK(gvr_get_maximum_effective_render_target_size(mPresentingContext));
  GVR_CHECK(gvr_buffer_spec_set_size(spec, mFrameBufferSize));
  GVR_CHECK(gvr_buffer_spec_set_samples(spec, 0));
  GVR_CHECK(gvr_buffer_spec_set_color_format(spec, GVR_COLOR_FORMAT_RGBA_8888));
  GVR_CHECK(gvr_buffer_spec_set_depth_stencil_format(spec, GVR_DEPTH_STENCIL_FORMAT_NONE));
  mSwapChain = GVR_CHECK(gvr_swap_chain_create(mPresentingContext, (const gvr_buffer_spec**)&spec, 1));
  GVR_CHECK(gvr_buffer_spec_destroy(&spec));
}

void
VRDisplayGVR::EnableControllers(const bool aEnable, VRSystemManager* aManager)
{
  if (aEnable && !mControllerAdded) {
    // Sometimes the gamepad doesn't get removed cleanly so just try to remove it before adding it.
    aManager->RemoveGamepad(mController->GetControllerInfo().mControllerID);
    aManager->AddGamepad(mController->GetControllerInfo());
    mControllerAdded = true;
  } else if (!aEnable && mControllerAdded) {
    mControllerAdded = false;
    aManager->RemoveGamepad(mController->GetControllerInfo().mControllerID);
  }

  gvr_context* context = mPresentingContext;

  if (!context) {
    if (mControllerContext) {
      GVR_CHECK(gvr_controller_destroy(&mControllerContext));
    }
    return;
  }

  if ((aEnable && mControllerContext) || (!aEnable && !mControllerContext)) {
    return;
  }

  if (aEnable) {
    if (!mControllerContext) {
      int32_t options = GVR_CHECK(gvr_controller_get_default_options());
      options |= GVR_CONTROLLER_ENABLE_GYRO | GVR_CONTROLLER_ENABLE_ACCEL | GVR_CONTROLLER_ENABLE_ARM_MODEL;
      mControllerContext = GVR_CHECK(gvr_controller_create_and_init(options, context));
      GVR_CHECK(gvr_controller_resume(mControllerContext));
    }
    if (!mControllerState) {
      mControllerState = GVR_CHECK(gvr_controller_state_create());
    }
  } else {
    GVR_CHECK(gvr_controller_pause(mControllerContext));
    GVR_CHECK(gvr_controller_destroy(&mControllerContext));
  }
}

void
VRDisplayGVR::UpdateControllers(VRSystemManager* aManager)
{
  if (!mControllerContext) {
    return;
  }

  GVR_CHECK(gvr_controller_apply_arm_model(mControllerContext, GVR_CONTROLLER_RIGHT_HANDED,GVR_ARM_MODEL_FOLLOW_GAZE, mHeadMatrix));
  GVR_CHECK(gvr_controller_state_update(mControllerContext, 0, mControllerState));
  mController->Update(mControllerState, aManager);
}


void
VRDisplayGVR::GetControllers(nsTArray<RefPtr<VRControllerHost> >& aControllerResult)
{
  aControllerResult.AppendElement(mController.get());
}

VRControllerGVR::VRControllerGVR(dom::GamepadHand aHand, uint32_t aDisplayID)
  : VRControllerHost(VRDeviceType::GVR, aHand, aDisplayID)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerGVR, VRControllerHost);
  mControllerInfo.mControllerName.AssignLiteral("Daydream Controller");
  // The gvr_controller_button enum starts with GVR_CONTROLLER_BUTTON_NONE at index zero
  // so the GVR controller has one less button than GVR_CONTROLLER_BUTTON_COUNT specifies.
  mControllerInfo.mNumButtons = GVR_CONTROLLER_BUTTON_COUNT - 1; // Skip dummy none button
  mControllerInfo.mNumAxes = 2;
  mControllerInfo.mNumHaptics = 0;
}

VRControllerGVR::~VRControllerGVR()
{
  MOZ_COUNT_DTOR_INHERITED(VRControllerGVR, VRControllerHost);
}

void
VRControllerGVR::Update(gvr_controller_state* aState, VRSystemManager* aManager)
{
  mPose.Clear();

  if (gvr_controller_state_get_connection_state(aState) != GVR_CONTROLLER_CONNECTED) {
    return;
  }
  const uint64_t previousPressMask = GetButtonPressed();
  const uint64_t previousTouchMask = GetButtonTouched();
  uint64_t currentPressMask = 0;
  uint64_t currentTouchMask = 0;
  // Index 0 is the dummy button so skip it.
  for (int ix = 1; ix < GVR_CONTROLLER_BUTTON_COUNT; ix++) {
    const uint64_t buttonMask = 0x01 << (ix - 1);
    bool pressed = gvr_controller_state_get_button_state(aState, ix);
    bool touched = pressed;
    if (ix == GVR_CONTROLLER_BUTTON_CLICK) {
       touched = gvr_controller_state_is_touching(aState);
       double xAxis = 0.0;
       double yAxis = 0.0;
       if (touched) {
         gvr_vec2f axes = gvr_controller_state_get_touch_pos(aState);
         xAxis = (axes.x * 2.0) - 1.0;
         yAxis = (axes.y * 2.0) - 1.0;
       }
       aManager->NewAxisMove(0, 0, xAxis);
       aManager->NewAxisMove(0, 1, yAxis);
    }
    if (pressed) {
      currentPressMask |= buttonMask;
    }
    if (touched) {
      currentTouchMask |= buttonMask;
    }
    if (((currentPressMask & buttonMask) ^ (previousPressMask & buttonMask)) ||
        ((currentTouchMask & buttonMask) ^ (previousTouchMask & buttonMask))) {
      aManager->NewButtonEvent(0, ix - 1, pressed, touched, pressed ? 1.0 : 0.0);
    }
  }
  SetButtonPressed(currentPressMask);
  SetButtonTouched(currentTouchMask);

  mPose.flags = dom::GamepadCapabilityFlags::Cap_Orientation | dom::GamepadCapabilityFlags::Cap_Position | dom::GamepadCapabilityFlags::Cap_LinearAcceleration;

  gvr_quatf ori = gvr_controller_state_get_orientation(aState);
  mPose.orientation[0] = ori.qx;
  mPose.orientation[1] = ori.qy;
  mPose.orientation[2] = ori.qz;
  mPose.orientation[3] = ori.qw;
  mPose.isOrientationValid = true;

  gvr_vec3f acc = gvr_controller_state_get_accel(aState);
  mPose.linearAcceleration[0] = acc.x;
  mPose.linearAcceleration[1] = acc.y;
  mPose.linearAcceleration[2] = acc.z;

  gvr_vec3f vel = gvr_controller_state_get_gyro(aState);
  mPose.angularVelocity[0] = vel.x;
  mPose.angularVelocity[1] = vel.y;
  mPose.angularVelocity[2] = vel.z;

  gvr_vec3f pos =  gvr_controller_state_get_position(aState);
  mPose.position[0] = pos.x;
  mPose.position[1] = pos.y;
  mPose.position[2] = pos.z;

  aManager->NewPoseState(0, mPose);
}

/*static*/ already_AddRefed<VRSystemManagerGVR>
VRSystemManagerGVR::Create()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gfxPrefs::VREnabled()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerGVR> manager = new VRSystemManagerGVR();
  return manager.forget();
}

void
VRSystemManagerGVR::Destroy()
{

}

void
VRSystemManagerGVR::Shutdown()
{

}

void
VRSystemManagerGVR::Enumerate()
{
  if (!GeckoVRManager::IsGVRPresent()) {
    return;
  }

  if (!mGVRHMD) {
    mGVRHMD = new VRDisplayGVR();
  }
}

bool
VRSystemManagerGVR::ShouldInhibitEnumeration()
{
  if (VRSystemManager::ShouldInhibitEnumeration()) {
    return true;
  }
  if (mGVRHMD) {
    // When we find an a VR device, don't
    // allow any further enumeration as it
    // may get picked up redundantly by other
    // API's.
    return true;
  }
  return false;
}

void
VRSystemManagerGVR::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (mGVRHMD) {
    aHMDResult.AppendElement(mGVRHMD);
  }
}

bool
VRSystemManagerGVR::GetIsPresenting()
{
  if (!mGVRHMD) {
    return false;
  }

  VRDisplayInfo displayInfo(mGVRHMD->GetDisplayInfo());
  return displayInfo.GetPresentingGroups() != kVRGroupNone;
}

void
VRSystemManagerGVR::HandleInput()
{
  if (mGVRHMD) {
    mGVRHMD->UpdateControllers(this);
  }
}

void
VRSystemManagerGVR::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
{
  if (mGVRHMD) {
    mGVRHMD->GetControllers(aControllerResult);
  }
}

void
VRSystemManagerGVR::ScanForControllers()
{
  if (mGVRHMD) {
    mGVRHMD->EnableControllers(true, this);
  }
}

void
VRSystemManagerGVR::RemoveControllers()
{
  if (mGVRHMD) {
    mGVRHMD->EnableControllers(false, this);
  }
}

void
VRSystemManagerGVR::VibrateHaptic(uint32_t aControllerIdx,
                                  uint32_t aHapticIndex,
                                  double aIntensity,
                                  double aDuration,
                                  const VRManagerPromise& aPromise)
{

}

void
VRSystemManagerGVR::StopVibrateHaptic(uint32_t aControllerIdx)
{

}

VRSystemManagerGVR::VRSystemManagerGVR()
  : mGVRHMD(nullptr)
{

}

VRSystemManagerGVR::~VRSystemManagerGVR()
{

}

