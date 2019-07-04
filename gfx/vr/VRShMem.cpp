/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRShMem.h"

#ifdef MOZILLA_INTERNAL_API
#  include "nsString.h"
#endif

#include "gfxVRMutex.h"

#if defined(XP_MACOSX)
#  include <sys/mman.h>
#  include <sys/stat.h> /* For mode constants */
#  include <fcntl.h>    /* For O_* constants */
#elif defined(MOZ_WIDGET_ANDROID)
#  include "GeckoVRManager.h"
#endif

#if !defined(XP_WIN)
#  include <unistd.h>  // for ::sleep
#endif

using namespace mozilla::gfx;

// TODO: we might need to use different names for the mutexes
// and mapped files if we have both release and nightlies
// running at the same time? Or...what if we have multiple
// release builds running on same machine? (Bug 1563232)
#ifdef XP_WIN
static const char* kShmemName = "moz.gecko.vr_ext.0.0.1";
#elif defined(XP_MACOSX)
static const char* kShmemName = "/moz.gecko.vr_ext.0.0.1";
#endif  //  XP_WIN

#if !defined(MOZ_WIDGET_ANDROID)
namespace {
void YieldThread() {
#  if defined(XP_WIN)
  ::Sleep(0);
#  else
  ::sleep(0);
#  endif
}
}  // anonymous namespace
#endif  // !defined(MOZ_WIDGET_ANDROID)

VRShMem::VRShMem(volatile VRExternalShmem* aShmem, bool aSameProcess,
                 bool aIsParentProcess)
    : mExternalShmem(aShmem),
      mSameProcess(aSameProcess)
#if defined(XP_WIN)
      ,
      mIsParentProcess(aIsParentProcess)
#endif
#if defined(XP_MACOSX)
      ,
      mShmemFD(0)
#elif defined(XP_WIN)
      ,
      mShmemFile(nullptr),
      mMutex(nullptr)
#endif
{
  // To confirm:
  // - aShmem is null for VRManager, or for service in multi-proc
  // - aShmem is !null for VRService in-proc
  // make this into an assert
  if (!(aShmem == nullptr || aSameProcess)) {
    //::DebugBreak();
  }

  // copied from VRService Ctor
  // When we have the VR process, we map the memory
  // of mAPIShmem from GPU process and pass it to the CTOR.
  // If we don't have the VR process, we will instantiate
  // mAPIShmem in VRService.
}

// Callers/Processes to CreateShMem should followup with CloseShMem
// [copied from VRManager::OpenShmem]
void VRShMem::CreateShMem() {
  if (mExternalShmem) {
    return;
  }
#if defined(XP_WIN)
  if (mMutex == nullptr) {
    mMutex = CreateMutex(nullptr,  // default security descriptor
                         false,    // mutex not owned
                         TEXT("mozilla::vr::ShmemMutex"));  // object name
    if (mMutex == nullptr) {
#  ifdef MOZILLA_INTERNAL_API
      nsAutoCString msg;
      msg.AppendPrintf("VRManager CreateMutex error \"%lu\".", GetLastError());
      NS_WARNING(msg.get());
#  endif
      MOZ_ASSERT(false);
      return;
    }
    // At xpcshell extension tests, it creates multiple VRManager
    // instances in plug-contrainer.exe. It causes GetLastError() return
    // `ERROR_ALREADY_EXISTS`. However, even though `ERROR_ALREADY_EXISTS`, it
    // still returns the same mutex handle.
    //
    // https://docs.microsoft.com/en-us/windows/desktop/api/synchapi/nf-synchapi-createmutexa
    MOZ_ASSERT(GetLastError() == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
  }
#endif  // XP_WIN
#if !defined(MOZ_WIDGET_ANDROID)
  // The VR Service accesses all hardware from a separate process
  // and replaces the other VRManager when enabled.
  // If the VR process is not enabled, create an in-process VRService.
  if (mSameProcess) {
    // If the VR process is disabled, attempt to create a
    // VR service within the current process
    mExternalShmem = new VRExternalShmem();
#  ifdef MOZILLA_INTERNAL_API
    // TODO: Create external variant to Clear (Bug 1563233)
    // VRExternalShmem is asserted to be POD
    mExternalShmem->Clear();
#  endif
    // TODO: move this call to the caller (Bug 1563233)
    // mServiceHost->CreateService(mExternalShmem);
    return;
  }
#endif

#if defined(XP_MACOSX)
  if (mShmemFD == 0) {
    mShmemFD =
        shm_open(kShmemName, O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
  }
  if (mShmemFD <= 0) {
    mShmemFD = 0;
    return;
  }

  struct stat sb;
  fstat(mShmemFD, &sb);
  off_t length = sb.st_size;
  if (length < (off_t)sizeof(VRExternalShmem)) {
    // TODO - Implement logging (Bug 1558912)
    CloseShMem();
    return;
  }

  mExternalShmem = (VRExternalShmem*)mmap(NULL, length, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, mShmemFD, 0);
  if (mExternalShmem == MAP_FAILED) {
    // TODO - Implement logging (Bug 1558912)
    mExternalShmem = NULL;
    CloseShMem();
    return;
  }

#elif defined(XP_WIN)
  if (mShmemFile == nullptr) {
    mShmemFile =
        CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(VRExternalShmem), kShmemName);
    MOZ_ASSERT(GetLastError() == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
    MOZ_ASSERT(mShmemFile);
    if (mShmemFile == nullptr) {
      // TODO - Implement logging (Bug 1558912)
      CloseShMem();
      return;
    }
  }
  LARGE_INTEGER length;
  length.QuadPart = sizeof(VRExternalShmem);
  mExternalShmem = (VRExternalShmem*)MapViewOfFile(
      mShmemFile,           // handle to map object
      FILE_MAP_ALL_ACCESS,  // read/write permission
      0, 0, length.QuadPart);

  if (mExternalShmem == nullptr) {
    // TODO - Implement logging (Bug 1558912)
    CloseShMem();
    return;
  }
#elif defined(MOZ_WIDGET_ANDROID) && defined(MOZILLA_INTERNAL_API)
  mExternalShmem =
      (VRExternalShmem*)mozilla::GeckoVRManager::GetExternalContext();
  if (!mExternalShmem) {
    return;
  }
  int32_t version = -1;
  int32_t size = 0;
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
      0) {
    version = mExternalShmem->version;
    size = mExternalShmem->size;
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
  } else {
    return;
  }
  if (version != kVRExternalVersion) {
    mExternalShmem = nullptr;
    return;
  }
  if (size != sizeof(VRExternalShmem)) {
    mExternalShmem = nullptr;
    return;
  }
#endif
}

// The cleanup corresponding to CreateShMem
// [copied from VRManager::CloseShmem, dtor]
void VRShMem::CloseShMem() {
#if !defined(MOZ_WIDGET_ANDROID)
  if (mSameProcess) {
    if (mExternalShmem) {
      delete mExternalShmem;
      mExternalShmem = nullptr;
    }
    return;
  }
#endif
#if defined(XP_MACOSX)
  if (mExternalShmem) {
    munmap((void*)mExternalShmem, sizeof(VRExternalShmem));
    mExternalShmem = NULL;
  }
  if (mShmemFD) {
    close(mShmemFD);
    mShmemFD = 0;
  }
#elif defined(XP_WIN)
  if (mExternalShmem) {
    UnmapViewOfFile((void*)mExternalShmem);
    mExternalShmem = nullptr;
  }
  if (mShmemFile) {
    CloseHandle(mShmemFile);
    mShmemFile = nullptr;
  }
#elif defined(MOZ_WIDGET_ANDROID)
  mExternalShmem = NULL;
#endif
}

// Called to use an existing shmem instance created by another process
// Callers to JoinShMem should call LeaveShMem for cleanup
// [copied from VRService::InitShmem, VRService::Start]
bool VRShMem::JoinShMem() {
  // was if (!mVRProcessEnabled) {
  if (mSameProcess) {
    return true;
  }

#if defined(XP_WIN)
  const char* kShmemName = "moz.gecko.vr_ext.0.0.1";
  base::ProcessHandle targetHandle = 0;

  // Opening a file-mapping object by name
  targetHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS,  // read/write access
                                  FALSE,        // do not inherit the name
                                  kShmemName);  // name of mapping object

  MOZ_ASSERT(GetLastError() == 0);

  LARGE_INTEGER length;
  length.QuadPart = sizeof(VRExternalShmem);
  mExternalShmem = (VRExternalShmem*)MapViewOfFile(
      reinterpret_cast<base::ProcessHandle>(
          targetHandle),    // handle to map object
      FILE_MAP_ALL_ACCESS,  // read/write permission
      0, 0, length.QuadPart);
  MOZ_ASSERT(GetLastError() == 0);
  // TODO - Implement logging (Bug 1558912)
  mShmemFile = targetHandle;
  if (!mExternalShmem) {
    MOZ_ASSERT(mExternalShmem);
    return false;
  }
#else
  // TODO: Implement shmem for other platforms.
  //
  // TODO: ** Does this mean that ShMem only works in Windows for now? If so,
  // let's delete the code from other platforms (Bug 1563234)
#endif

#if defined(XP_WIN)
  // Adding `!XRE_IsParentProcess()` to avoid Win 7 32-bit WebVR tests
  // to OpenMutex when there is no GPU process to create
  // VRSystemManagerExternal and its mutex.
  // was if (!mMutex && !XRE_IsParentProcess()) {
  if (!mMutex && !mIsParentProcess) {
    mMutex = OpenMutex(MUTEX_ALL_ACCESS,  // request full access
                       false,             // handle not inheritable
                       TEXT("mozilla::vr::ShmemMutex"));  // object name

    if (mMutex == nullptr) {
#  ifdef MOZILLA_INTERNAL_API
      nsAutoCString msg;
      msg.AppendPrintf("VRService OpenMutex error \"%lu\".", GetLastError());
      NS_WARNING(msg.get());
#  endif
      MOZ_ASSERT(false);
    }
    MOZ_ASSERT(GetLastError() == 0);
  }
#endif

  return true;
}

// The cleanup corresponding to JoinShMem
// [copied from VRService::Stop]
void VRShMem::LeaveShMem() {
#if defined(XP_WIN)
  if (mShmemFile) {
    ::CloseHandle(mShmemFile);
    mShmemFile = nullptr;
  }
#endif

  // was if (mVRProcessEnabled && mAPIShmem) {
  if (mExternalShmem != nullptr && !mSameProcess) {
#if defined(XP_WIN)
    UnmapViewOfFile((void*)mExternalShmem);
#endif
    mExternalShmem = nullptr;
  }
#if defined(XP_WIN)
  if (mMutex) {
    CloseHandle(mMutex);
    mMutex = nullptr;
  }
#endif
}

// [copied from from VRManager::PushState]
void VRShMem::PushBrowserState(VRBrowserState& aBrowserState,
                               bool aNotifyCond) {
  if (!mExternalShmem) {
    return;
  }
#if defined(MOZ_WIDGET_ANDROID)
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->geckoMutex)) ==
      0) {
    memcpy((void*)&(mExternalShmem->geckoState), (void*)&aBrowserState,
           sizeof(VRBrowserState));
    if (aNotifyCond) {
      pthread_cond_signal((pthread_cond_t*)&(mExternalShmem->geckoCond));
    }
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->geckoMutex));
  }
#else
  bool status = true;
#  if defined(XP_WIN)
  WaitForMutex lock(mMutex);
  status = lock.GetStatus();
#  endif  // defined(XP_WIN)
  if (status) {
    mExternalShmem->geckoGenerationA++;
    memcpy((void*)&(mExternalShmem->geckoState), (void*)&aBrowserState,
           sizeof(VRBrowserState));
    mExternalShmem->geckoGenerationB++;
  }
#endif    // defined(MOZ_WIDGET_ANDROID)
}

// [copied from VRService::PullState]
void VRShMem::PullBrowserState(mozilla::gfx::VRBrowserState& aState) {
  if (!mExternalShmem) {
    return;
  }
  // Copying the browser state from the shmem is non-blocking
  // on x86/x64 architectures.  Arm requires a mutex that is
  // locked for the duration of the memcpy to and from shmem on
  // both sides.
  // On x86/x64 It is fallable -- If a dirty copy is detected by
  // a mismatch of geckoGenerationA and geckoGenerationB,
  // the copy is discarded and will not replace the last known
  // browser state.

#if defined(MOZ_WIDGET_ANDROID)
  // TODO: This code is out-of-date and fails to compile, as
  // VRService isn't compiled for Android (Bug 1563234)
  /*
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->geckoMutex)) ==
      0) {
    memcpy(&aState, &tmp.geckoState, sizeof(VRBrowserState));
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->geckoMutex));
  }
  */
#else
  bool status = true;
#  if defined(XP_WIN)
  // was if (!XRE_IsParentProcess()) {
  if (!mIsParentProcess) {
    // TODO: Is this scoped lock okay? Seems like it should allow some
    // race condition (Bug 1563234)
    WaitForMutex lock(mMutex);
    status = lock.GetStatus();
  }
#  endif  // defined(XP_WIN)
  if (status) {
    VRExternalShmem tmp;
    if (mExternalShmem->geckoGenerationA != mBrowserGeneration) {
      // TODO - (void *) cast removes volatile semantics.
      // The memcpy is not likely to be optimized out, but is theoretically
      // possible.  Suggest refactoring to either explicitly enforce memory
      // order or to use locks.
      memcpy(&tmp, (void*)mExternalShmem, sizeof(VRExternalShmem));
      if (tmp.geckoGenerationA == tmp.geckoGenerationB &&
          tmp.geckoGenerationA != 0) {
        memcpy(&aState, &tmp.geckoState, sizeof(VRBrowserState));
        mBrowserGeneration = tmp.geckoGenerationA;
      }
    }
  }
#endif    // defined(MOZ_WIDGET_ANDROID)
}

// [copied from VRService::PushState]
void VRShMem::PushSystemState(const mozilla::gfx::VRSystemState& aState) {
  if (!mExternalShmem) {
    return;
  }
  // Copying the VR service state to the shmem is atomic, infallable,
  // and non-blocking on x86/x64 architectures.  Arm requires a mutex
  // that is locked for the duration of the memcpy to and from shmem on
  // both sides.

#if defined(MOZ_WIDGET_ANDROID)
  // TODO: This code is out-of-date and fails to compile, as
  // VRService isn't compiled for Android (Bug 1563234)
  /*
  if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
      0) {
    // We are casting away the volatile keyword, which is not accepted by
    // memcpy.  It is possible (although very unlikely) that the compiler
    // may optimize out the memcpy here as memcpy isn't explicitly safe for
    // volatile memory in the C++ standard.
    memcpy((void*)&mExternalShmem->state, &aState, sizeof(VRSystemState));
    pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
  }
  */
#else
  bool lockState = true;
#  if defined(XP_WIN)
  // was if (!XRE_IsParentProcess()) {
  if (!mIsParentProcess) {
    WaitForMutex lock(mMutex);
    lockState = lock.GetStatus();
  }
#  endif  // defined(XP_WIN)
  if (lockState) {
    mExternalShmem->generationA++;
    memcpy((void*)&mExternalShmem->state, &aState, sizeof(VRSystemState));
    mExternalShmem->generationB++;
  }
#endif    // defined(MOZ_WIDGET_ANDROID)
}

// [copied from void VRManager::PullState]
#if defined(MOZ_WIDGET_ANDROID)
void VRShMem::PullSystemState(
    VRDisplayState& aDisplayState, VRHMDSensorState& aSensorState,
    VRControllerState (&aControllerState)[kVRControllerMaxCount],
    bool& aEnumerationCompleted,
    const std::function<bool()>& aWaitCondition /* = nullptr */) {
  if (!mExternalShmem) {
    return;
  }
  bool done = false;
  while (!done) {
    if (pthread_mutex_lock((pthread_mutex_t*)&(mExternalShmem->systemMutex)) ==
        0) {
      while (true) {
        memcpy(&aDisplayState, (void*)&(mExternalShmem->state.displayState),
               sizeof(VRDisplayState));
        memcpy(&aSensorState, (void*)&(mExternalShmem->state.sensorState),
               sizeof(VRHMDSensorState));
        memcpy(aControllerState,
               (void*)&(mExternalShmem->state.controllerState),
               sizeof(VRControllerState) * kVRControllerMaxCount);
        aEnumerationCompleted = mExternalShmem->state.enumerationCompleted;
        if (!aWaitCondition || aWaitCondition()) {
          done = true;
          break;
        }
        // Block current thead using the condition variable until data
        // changes
        pthread_cond_wait((pthread_cond_t*)&mExternalShmem->systemCond,
                          (pthread_mutex_t*)&mExternalShmem->systemMutex);
      }  // while (true)
      pthread_mutex_unlock((pthread_mutex_t*)&(mExternalShmem->systemMutex));
    } else if (!aWaitCondition) {
      // pthread_mutex_lock failed and we are not waiting for a condition to
      // exit from PullState call.
      return;
    }
  }  // while (!done) {
}
#else
void VRShMem::PullSystemState(
    VRDisplayState& aDisplayState, VRHMDSensorState& aSensorState,
    VRControllerState (&aControllerState)[kVRControllerMaxCount],
    bool& aEnumerationCompleted,
    const std::function<bool()>& aWaitCondition /* = nullptr */) {
  MOZ_ASSERT(mExternalShmem);
  if (!mExternalShmem) {
    return;
  }
  while (true) {
    {  // Scope for WaitForMutex
#  if defined(XP_WIN)
      bool status = true;
      WaitForMutex lock(mMutex);
      status = lock.GetStatus();
      if (status) {
#  endif  // defined(XP_WIN)
        VRExternalShmem tmp;
        memcpy(&tmp, (void*)mExternalShmem, sizeof(VRExternalShmem));
        bool isCleanCopy =
            tmp.generationA == tmp.generationB && tmp.generationA != 0;
        if (isCleanCopy) {
          memcpy(&aDisplayState, &tmp.state.displayState,
                 sizeof(VRDisplayState));
          memcpy(&aSensorState, &tmp.state.sensorState,
                 sizeof(VRHMDSensorState));
          memcpy(aControllerState,
                 (void*)&(mExternalShmem->state.controllerState),
                 sizeof(VRControllerState) * kVRControllerMaxCount);
          aEnumerationCompleted = mExternalShmem->state.enumerationCompleted;
          // Check for wait condition
          if (!aWaitCondition || aWaitCondition()) {
            return;
          }
        }  // if (isCleanCopy)
        // Yield the thread while polling
        YieldThread();
#  if defined(XP_WIN)
      } else if (!aWaitCondition) {
        // WaitForMutex failed and we are not waiting for a condition to
        // exit from PullState call.
        return;
      }
#  endif  // defined(XP_WIN)
    }  // End: Scope for WaitForMutex
    // Yield the thread while polling
    YieldThread();
  }  // while (!true)
}
#endif    // defined(MOZ_WIDGET_ANDROID)
