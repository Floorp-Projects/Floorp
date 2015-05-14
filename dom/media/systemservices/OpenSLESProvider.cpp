/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenSLESProvider.h"
#include "prlog.h"
#include "nsDebug.h"

#include <dlfcn.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

// NSPR_LOG_MODULES=OpenSLESProvider:5
#undef LOG
#undef LOG_ENABLED
PRLogModuleInfo *gOpenSLESProviderLog;
#define LOG(args) PR_LOG(gOpenSLESProviderLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gOpenSLESProviderLog, PR_LOG_DEBUG)

namespace mozilla {

OpenSLESProvider::OpenSLESProvider()
  : mLock("OpenSLESProvider.mLock"),
    mSLEngine(nullptr),
    mSLEngineUsers(0),
    mIsRealized(false),
    mOpenSLESLib(nullptr)
{
  if (!gOpenSLESProviderLog)
    gOpenSLESProviderLog = PR_NewLogModule("OpenSLESProvider");
  LOG(("OpenSLESProvider being initialized"));
}

OpenSLESProvider::~OpenSLESProvider()
{
  if (mOpenSLESLib) {
    LOG(("OpenSLES Engine was not properly Destroyed"));
    (void)dlclose(mOpenSLESLib);
  }
}

/* static */
OpenSLESProvider& OpenSLESProvider::getInstance()
{
  // This doesn't need a Mutex in C++11 or GCC 4.3+, see N2660 and
  // https://gcc.gnu.org/projects/cxx0x.html
  static OpenSLESProvider instance;
  return instance;
}

/* static */
SLresult OpenSLESProvider::Get(SLObjectItf * aObjectm,
                               SLuint32 aOptionCount,
                               const SLEngineOption *aOptions)
{
  OpenSLESProvider& provider = OpenSLESProvider::getInstance();
  return provider.GetEngine(aObjectm, aOptionCount, aOptions);
}

SLresult OpenSLESProvider::GetEngine(SLObjectItf * aObjectm,
                                     SLuint32 aOptionCount,
                                     const SLEngineOption *aOptions)
{
  MutexAutoLock lock(mLock);
  LOG(("Getting OpenSLES engine"));
  // Bug 1042051: Validate options are the same
  if (mSLEngine != nullptr) {
    *aObjectm = mSLEngine;
    mSLEngineUsers++;
    LOG(("Returning existing engine, %d users", mSLEngineUsers));
    return SL_RESULT_SUCCESS;
  } else {
    int res = ConstructEngine(aObjectm, aOptionCount, aOptions);
    if (res == SL_RESULT_SUCCESS) {
      // Bug 1042051: Store engine options
      mSLEngine = *aObjectm;
      mSLEngineUsers++;
      LOG(("Returning new engine"));
    } else {
      LOG(("Error getting engine: %d", res));
    }
    return res;
  }
}

SLresult OpenSLESProvider::ConstructEngine(SLObjectItf * aObjectm,
                                           SLuint32 aOptionCount,
                                           const SLEngineOption *aOptions)
{
  mLock.AssertCurrentThreadOwns();

  if (!mOpenSLESLib) {
    mOpenSLESLib = dlopen("libOpenSLES.so", RTLD_LAZY);
    if (!mOpenSLESLib) {
      LOG(("Failed to dlopen OpenSLES library"));
      return SL_RESULT_MEMORY_FAILURE;
    }
  }

  typedef SLresult (*slCreateEngine_t)(SLObjectItf *,
                                       SLuint32,
                                       const SLEngineOption *,
                                       SLuint32,
                                       const SLInterfaceID *,
                                       const SLboolean *);

  slCreateEngine_t f_slCreateEngine =
    (slCreateEngine_t)dlsym(mOpenSLESLib, "slCreateEngine");
  int result = f_slCreateEngine(aObjectm, aOptionCount, aOptions, 0, NULL, NULL);
  return result;
}

/* static */
void OpenSLESProvider::Destroy(SLObjectItf * aObjectm)
{
  OpenSLESProvider& provider = OpenSLESProvider::getInstance();
  provider.DestroyEngine(aObjectm);
}

void OpenSLESProvider::DestroyEngine(SLObjectItf * aObjectm)
{
  MutexAutoLock lock(mLock);
  NS_ASSERTION(mOpenSLESLib, "OpenSLES destroy called but library is not open");

  mSLEngineUsers--;
  LOG(("Freeing engine, %d users left", mSLEngineUsers));
  if (mSLEngineUsers) {
    return;
  }

  (*(*aObjectm))->Destroy(*aObjectm);
  // This assumes SLObjectItf is a pointer, but given the previous line,
  // that's a given.
  *aObjectm = nullptr;

  (void)dlclose(mOpenSLESLib);
  mOpenSLESLib = nullptr;
  mIsRealized = false;
}

/* static */
SLresult OpenSLESProvider::Realize(SLObjectItf aObjectm)
{
  OpenSLESProvider& provider = OpenSLESProvider::getInstance();
  return provider.RealizeEngine(aObjectm);
}

SLresult OpenSLESProvider::RealizeEngine(SLObjectItf aObjectm)
{
  MutexAutoLock lock(mLock);
  NS_ASSERTION(mOpenSLESLib, "OpenSLES realize called but library is not open");
  NS_ASSERTION(aObjectm != nullptr, "OpenSLES realize engine with empty ObjectItf");

  if (mIsRealized) {
    LOG(("Not realizing already realized engine"));
    return SL_RESULT_SUCCESS;
  } else {
    SLresult res = (*aObjectm)->Realize(aObjectm, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) {
      LOG(("Error realizing OpenSLES engine: %d", res));
    } else {
      LOG(("Realized OpenSLES engine"));
      mIsRealized = true;
    }
    return res;
  }
}

} // namespace mozilla

extern "C" {
SLresult mozilla_get_sles_engine(SLObjectItf * aObjectm,
                                 SLuint32 aOptionCount,
                                 const SLEngineOption *aOptions)
{
  return mozilla::OpenSLESProvider::Get(aObjectm, aOptionCount, aOptions);
}

void mozilla_destroy_sles_engine(SLObjectItf * aObjectm)
{
  mozilla::OpenSLESProvider::Destroy(aObjectm);
}

SLresult mozilla_realize_sles_engine(SLObjectItf aObjectm)
{
  return mozilla::OpenSLESProvider::Realize(aObjectm);
}

}

