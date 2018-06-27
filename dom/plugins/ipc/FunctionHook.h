/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_ipc_functionhook_h
#define dom_plugins_ipc_functionhook_h 1

#include "IpdlTuple.h"
#include "base/process.h"
#include "mozilla/Atomics.h"

#if defined(XP_WIN)
#include "nsWindowsDllInterceptor.h"
#endif

namespace mozilla {
namespace plugins {

// "PluginHooks" logging helpers
extern mozilla::LazyLogModule sPluginHooksLog;
#define HOOK_LOG(lvl, msg) MOZ_LOG(mozilla::plugins::sPluginHooksLog, lvl, msg);
inline const char *SuccessMsg(bool aVal) { return aVal ? "succeeded" : "failed";  }

class FunctionHook;
class FunctionHookArray;

class FunctionHook
{
public:
  virtual ~FunctionHook() {}

  virtual FunctionHookId FunctionId() const = 0;

  /**
   * Register to hook the function represented by this class.
   * Returns false if we should have hooked but didn't.
   */
  virtual bool Register(int aQuirks) = 0;

  /**
   * Run the original function with parameters stored in a tuple.
   * This is only supported on server-side and for auto-brokered methods.
   */
  virtual bool RunOriginalFunction(base::ProcessId aClientId,
                                   const IPC::IpdlTuple &aInTuple,
                                   IPC::IpdlTuple *aOutTuple) const = 0;

  /**
   * Hook the Win32 methods needed by the plugin process.
   */
  static void HookFunctions(int aQuirks);

  static FunctionHookArray* GetHooks();

#if defined(XP_WIN)
  /**
   * Special handler for hooking some kernel32.dll methods that we use to
   * disable Flash protected mode.
   */
  static void HookProtectedMode();

  /**
   * Get the WindowsDllInterceptor for the given module.  Creates a cache of
   * WindowsDllInterceptors by name.
   */
  static WindowsDllInterceptor* GetDllInterceptorFor(const char* aModuleName);

  /**
   * Must be called to clear the cache created by calls to GetDllInterceptorFor.
   */
  static void ClearDllInterceptorCache();
#endif // defined(XP_WIN)

private:
  static StaticAutoPtr<FunctionHookArray> sFunctionHooks;
  static void AddFunctionHooks(FunctionHookArray& aHooks);
};

// The FunctionHookArray deletes its FunctionHook objects when freed.
class FunctionHookArray : public nsTArray<FunctionHook*> {
public:
  ~FunctionHookArray()
  {
    for (uint32_t idx = 0; idx < Length(); ++idx) {
      FunctionHook* elt = ElementAt(idx);
      MOZ_ASSERT(elt);
      delete elt;
    }
  }
};

// Type of function that returns true if a function should be hooked according to quirks.
typedef bool(ShouldHookFunc)(int aQuirks);

template<FunctionHookId functionId, typename FunctionType>
class BasicFunctionHook : public FunctionHook
{
#if defined(XP_WIN)
  using FuncHookType = WindowsDllInterceptor::FuncHookType<FunctionType*>;
#endif // defined(XP_WIN)

public:
  BasicFunctionHook(const char* aModuleName,
                    const char* aFunctionName, FunctionType* aOldFunction,
                    FunctionType* aNewFunction)
    : mOldFunction(aOldFunction)
    , mRegistration(UNREGISTERED)
    , mModuleName(aModuleName)
    , mFunctionName(aFunctionName)
    , mNewFunction(aNewFunction)
  {
    MOZ_ASSERT(mOldFunction);
    MOZ_ASSERT(mNewFunction);
  }

  /**
   * Hooks the function if we haven't already and if ShouldHook() says to.
   */
  bool Register(int aQuirks) override;

  /**
   * Can be specialized to perform "extra" operations when running the
   * function on the server side.
   */
  bool RunOriginalFunction(base::ProcessId aClientId,
                           const IPC::IpdlTuple &aInTuple,
                           IPC::IpdlTuple *aOutTuple) const override { return false; }

  FunctionHookId FunctionId() const override { return functionId; }

  FunctionType* OriginalFunction() const { return mOldFunction; }

protected:
  // Once the function is hooked, this field will take the value of a pointer to
  // a function that performs the old behavior.  Before that, it is a pointer to
  // the original function.
  Atomic<FunctionType*> mOldFunction;
#if defined(XP_WIN)
  FuncHookType mStub;
#endif // defined(XP_WIN)

  enum RegistrationStatus { UNREGISTERED, FAILED, SUCCEEDED };
  RegistrationStatus mRegistration;

  // The name of the module containing the function to hook.  E.g. "user32.dll".
  const nsCString mModuleName;
  // The name of the function in the module.
  const nsCString mFunctionName;
  // The function that we should replace functionName with.  The signature of
  // newFunction must match that of functionName.
  FunctionType* const mNewFunction;
  static ShouldHookFunc* const mShouldHook;
};

// Default behavior is to hook every registered function.
extern bool AlwaysHook(int);
template<FunctionHookId functionId, typename FunctionType>
ShouldHookFunc* const BasicFunctionHook<functionId, FunctionType>::mShouldHook = AlwaysHook;

template <FunctionHookId functionId, typename FunctionType>
bool
BasicFunctionHook<functionId, FunctionType>::Register(int aQuirks)
{
  MOZ_RELEASE_ASSERT(XRE_IsPluginProcess());

  // If we have already attempted to hook this function or if quirks tell us
  // not to then don't hook.
  if (mRegistration != UNREGISTERED || !mShouldHook(aQuirks)) {
    return true;
  }

  bool isHooked = false;
  mRegistration = FAILED;

#if defined(XP_WIN)
  WindowsDllInterceptor* dllInterceptor =
    FunctionHook::GetDllInterceptorFor(mModuleName.Data());
  if (!dllInterceptor) {
    return false;
  }

  isHooked = mStub.Set(*dllInterceptor, mFunctionName.Data(), mNewFunction);
#endif

  if (isHooked) {
#if defined(XP_WIN)
    mOldFunction = mStub.GetStub();
#endif
    mRegistration = SUCCEEDED;
  }

  HOOK_LOG(LogLevel::Debug,
           ("Registering to intercept function '%s' : '%s'", mFunctionName.Data(),
            SuccessMsg(isHooked)));

  return isHooked;
}

}
}

#endif // dom_plugins_ipc_functionhook_h
