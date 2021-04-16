/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ModuleLoadFrame.h"

#include "LoaderPrivateAPI.h"

namespace mozilla {
namespace freestanding {

ModuleLoadFrame::ModuleLoadFrame(PCUNICODE_STRING aRequestedDllName)
    : mPrev(sTopFrame.get()),
      mContext(nullptr),
      mLSPSubstitutionRequired(false),
      mLoadNtStatus(STATUS_UNSUCCESSFUL),
      mLoadInfo(aRequestedDllName) {
  EnsureInitialized();
  sTopFrame.set(this);

  gLoaderPrivateAPI.NotifyBeginDllLoad(mLoadInfo, &mContext, aRequestedDllName);
}

ModuleLoadFrame::ModuleLoadFrame(nt::AllocatedUnicodeString&& aSectionName,
                                 const void* aMapBaseAddr, NTSTATUS aNtStatus,
                                 ModuleLoadInfo::Status aLoadStatus,
                                 bool aIsDependent)
    : mPrev(sTopFrame.get()),
      mContext(nullptr),
      mLSPSubstitutionRequired(false),
      mLoadNtStatus(aNtStatus),
      mLoadInfo(std::move(aSectionName), aMapBaseAddr, aLoadStatus,
                aIsDependent) {
  sTopFrame.set(this);

  gLoaderPrivateAPI.NotifyBeginDllLoad(&mContext, mLoadInfo.mSectionName);
}

ModuleLoadFrame::~ModuleLoadFrame() {
  gLoaderPrivateAPI.NotifyEndDllLoad(mContext, mLoadNtStatus,
                                     std::move(mLoadInfo));
  sTopFrame.set(mPrev);
}

/* static */
void ModuleLoadFrame::NotifyLSPSubstitutionRequired(
    PCUNICODE_STRING aLeafName) {
  ModuleLoadFrame* topFrame = sTopFrame.get();
  if (!topFrame) {
    return;
  }

  topFrame->SetLSPSubstitutionRequired(aLeafName);
}

void ModuleLoadFrame::SetLSPSubstitutionRequired(PCUNICODE_STRING aLeafName) {
  MOZ_ASSERT(!mLoadInfo.mBaseAddr);
  if (mLoadInfo.mBaseAddr) {
    // If mBaseAddr is not null then |this| has already seen a module load. This
    // should not be the case for a LSP substitution, so we bail.
    return;
  }

  // Save aLeafName, as it will be used by SetLoadStatus when invoking
  // SubstituteForLSP
  mLoadInfo.mRequestedDllName = aLeafName;
  mLSPSubstitutionRequired = true;
}

/* static */
void ModuleLoadFrame::NotifySectionMap(
    nt::AllocatedUnicodeString&& aSectionName, const void* aMapBaseAddr,
    NTSTATUS aMapNtStatus, ModuleLoadInfo::Status aLoadStatus,
    bool aIsDependent) {
  ModuleLoadFrame* topFrame = sTopFrame.get();
  if (!topFrame) {
    // The only time that this data is useful is during initial mapping of
    // the executable's dependent DLLs. If mozglue is present then
    // IsDefaultObserver will return false, indicating that we are beyond
    // initial process startup.
    if (gLoaderPrivateAPI.IsDefaultObserver()) {
      OnBareSectionMap(std::move(aSectionName), aMapBaseAddr, aMapNtStatus,
                       aLoadStatus, aIsDependent);
    }
    return;
  }

  topFrame->OnSectionMap(std::move(aSectionName), aMapBaseAddr, aMapNtStatus,
                         aLoadStatus, aIsDependent);
}

/* static */
bool ModuleLoadFrame::ExistsTopFrame() { return !!sTopFrame.get(); }

void ModuleLoadFrame::OnSectionMap(nt::AllocatedUnicodeString&& aSectionName,
                                   const void* aMapBaseAddr,
                                   NTSTATUS aMapNtStatus,
                                   ModuleLoadInfo::Status aLoadStatus,
                                   bool aIsDependent) {
  if (mLoadInfo.mBaseAddr) {
    // If mBaseAddr is not null then |this| has already seen a module load. This
    // means that we are witnessing a bare section map.
    OnBareSectionMap(std::move(aSectionName), aMapBaseAddr, aMapNtStatus,
                     aLoadStatus, aIsDependent);
    return;
  }

  mLoadInfo.mSectionName = std::move(aSectionName);
  mLoadInfo.mBaseAddr = aMapBaseAddr;
  mLoadInfo.mStatus = aLoadStatus;
}

/* static */
void ModuleLoadFrame::OnBareSectionMap(
    nt::AllocatedUnicodeString&& aSectionName, const void* aMapBaseAddr,
    NTSTATUS aMapNtStatus, ModuleLoadInfo::Status aLoadStatus,
    bool aIsDependent) {
  // We call the special constructor variant that is used for bare mappings.
  ModuleLoadFrame frame(std::move(aSectionName), aMapBaseAddr, aMapNtStatus,
                        aLoadStatus, aIsDependent);
}

NTSTATUS ModuleLoadFrame::SetLoadStatus(NTSTATUS aNtStatus,
                                        PHANDLE aOutHandle) {
  mLoadNtStatus = aNtStatus;

  if (!mLSPSubstitutionRequired) {
    return aNtStatus;
  }

  if (!gLoaderPrivateAPI.SubstituteForLSP(mLoadInfo.mRequestedDllName,
                                          aOutHandle)) {
    return aNtStatus;
  }

  return STATUS_SUCCESS;
}

SafeThreadLocal<ModuleLoadFrame*> ModuleLoadFrame::sTopFrame;

}  // namespace freestanding
}  // namespace mozilla
