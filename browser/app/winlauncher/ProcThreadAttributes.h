/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ProcThreadAttributes_h
#define mozilla_ProcThreadAttributes_h

#include <windows.h>

#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

namespace mozilla {

class MOZ_RAII ProcThreadAttributes final {
  struct ProcThreadAttributeListDeleter {
    void operator()(LPPROC_THREAD_ATTRIBUTE_LIST aList) {
      ::DeleteProcThreadAttributeList(aList);
      delete[] reinterpret_cast<char*>(aList);
    }
  };

  using ProcThreadAttributeListPtr =
      UniquePtr<_PROC_THREAD_ATTRIBUTE_LIST, ProcThreadAttributeListDeleter>;

 public:
  ProcThreadAttributes() : mMitigationPolicies(0) {}

  ~ProcThreadAttributes() = default;

  ProcThreadAttributes(const ProcThreadAttributes&) = delete;
  ProcThreadAttributes(ProcThreadAttributes&&) = delete;
  ProcThreadAttributes& operator=(const ProcThreadAttributes&) = delete;
  ProcThreadAttributes& operator=(ProcThreadAttributes&&) = delete;

  void AddMitigationPolicy(DWORD64 aPolicy) { mMitigationPolicies |= aPolicy; }

  bool AddInheritableHandle(HANDLE aHandle) {
    DWORD type = ::GetFileType(aHandle);
    if (type != FILE_TYPE_DISK && type != FILE_TYPE_PIPE) {
      return false;
    }

    if (!::SetHandleInformation(aHandle, HANDLE_FLAG_INHERIT,
                                HANDLE_FLAG_INHERIT)) {
      return false;
    }

    return mInheritableHandles.append(aHandle);
  }

  template <size_t N>
  bool AddInheritableHandles(HANDLE (&aHandles)[N]) {
    bool ok = true;
    for (auto handle : aHandles) {
      ok &= AddInheritableHandle(handle);
    }

    return ok;
  }

  bool HasMitigationPolicies() const { return !!mMitigationPolicies; }

  bool HasInheritableHandles() const { return !mInheritableHandles.empty(); }

  /**
   * @return false if the STARTUPINFOEXW::lpAttributeList was set to null
   *               as expected based on the state of |this|;
   *         true  if the STARTUPINFOEXW::lpAttributeList was set to
   *               non-null;
   */
  LauncherResult<bool> AssignTo(STARTUPINFOEXW& aSiex) {
    ZeroMemory(&aSiex, sizeof(STARTUPINFOEXW));

    // We'll set the size to sizeof(STARTUPINFOW) until we determine whether the
    // extended fields will be used.
    aSiex.StartupInfo.cb = sizeof(STARTUPINFOW);

    DWORD numAttributes = 0;
    if (HasMitigationPolicies()) {
      ++numAttributes;
    }

    if (HasInheritableHandles()) {
      ++numAttributes;
    }

    if (!numAttributes) {
      return false;
    }

    SIZE_T listSize = 0;
    if (!::InitializeProcThreadAttributeList(nullptr, numAttributes, 0,
                                             &listSize)) {
      DWORD err = ::GetLastError();
      if (err != ERROR_INSUFFICIENT_BUFFER) {
        return LAUNCHER_ERROR_FROM_WIN32(err);
      }
    }

    auto buf = MakeUnique<char[]>(listSize);

    LPPROC_THREAD_ATTRIBUTE_LIST tmpList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buf.get());

    if (!::InitializeProcThreadAttributeList(tmpList, numAttributes, 0,
                                             &listSize)) {
      return LAUNCHER_ERROR_FROM_LAST();
    }

    // Transfer buf to a ProcThreadAttributeListPtr - now that the list is
    // initialized, we are no longer dealing with a plain old char array. We
    // must now deinitialize the attribute list before deallocating the
    // underlying buffer.
    ProcThreadAttributeListPtr attrList(
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buf.release()));

    if (mMitigationPolicies) {
      if (!::UpdateProcThreadAttribute(
              attrList.get(), 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
              &mMitigationPolicies, sizeof(mMitigationPolicies), nullptr,
              nullptr)) {
        return LAUNCHER_ERROR_FROM_LAST();
      }
    }

    if (!mInheritableHandles.empty()) {
      if (!::UpdateProcThreadAttribute(
              attrList.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
              mInheritableHandles.begin(),
              mInheritableHandles.length() * sizeof(HANDLE), nullptr,
              nullptr)) {
        return LAUNCHER_ERROR_FROM_LAST();
      }
    }

    mAttrList = std::move(attrList);
    aSiex.lpAttributeList = mAttrList.get();
    aSiex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    return true;
  }

 private:
  static const uint32_t kNumInline = 3;  // Inline storage for the std handles

  DWORD64 mMitigationPolicies;
  Vector<HANDLE, kNumInline> mInheritableHandles;
  ProcThreadAttributeListPtr mAttrList;
};

}  // namespace mozilla

#endif  // mozilla_ProcThreadAttributes_h
