/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_LoaderPrivateAPI_h
#define mozilla_freestanding_LoaderPrivateAPI_h

#include "mozilla/LoaderAPIInterfaces.h"

namespace mozilla {
namespace freestanding {

/**
 * This part of the API is available only to the launcher process.
 */
class NS_NO_VTABLE LoaderPrivateAPI : public nt::LoaderAPI {
 public:
  /**
   * Notify the nt::LoaderObserver that a module load is beginning
   */
  virtual void NotifyBeginDllLoad(void** aContext,
                                  PCUNICODE_STRING aRequestedDllName) = 0;
  /**
   * Notify the nt::LoaderObserver that a module load is beginning and set the
   * begin load timestamp on |aModuleLoadInfo|.
   */
  virtual void NotifyBeginDllLoad(ModuleLoadInfo& aModuleLoadInfo,
                                  void** aContext,
                                  PCUNICODE_STRING aRequestedDllName) = 0;

  /**
   * Set a new nt::LoaderObserver to be used by the launcher process. NB: This
   * should only happen while the current process is still single-threaded!
   */
  virtual void SetObserver(nt::LoaderObserver* aNewObserver) = 0;

  /**
   * Returns true if the current nt::LoaderObserver is the launcher process's
   * built-in observer.
   */
  virtual bool IsDefaultObserver() const = 0;

  /**
   * Returns the name of a given mapped section address as a local instance of
   * nt::MemorySectionNameBuf.  This does not involve heap allocation.
   */
  virtual nt::MemorySectionNameBuf GetSectionNameBuffer(void* aSectionAddr) = 0;
};

/**
 * Ensures that any statics in the freestanding library are initialized.
 */
void EnsureInitialized();

extern LoaderPrivateAPI& gLoaderPrivateAPI;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_LoaderPrivateAPI_h
