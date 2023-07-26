/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include "base/logging.h"
#include "base/win_util.h"
#include "base/string_util.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/RandomNum.h"
#include "nsDebug.h"
#include "nsString.h"
#ifdef MOZ_MEMORY
#  include "mozmemory_utils.h"
#endif

namespace {
// NtQuerySection is an internal (but believed to be stable) API and the
// structures it uses are defined in nt_internals.h.
// So we have to define them ourselves.
typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation,
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef ULONG(__stdcall* NtQuerySectionType)(
    HANDLE SectionHandle, SECTION_INFORMATION_CLASS SectionInformationClass,
    PVOID SectionInformation, ULONG SectionInformationLength,
    PULONG ResultLength);

// Checks if the section object is safe to map. At the moment this just means
// it's not an image section.
bool IsSectionSafeToMap(HANDLE handle) {
  static NtQuerySectionType nt_query_section_func =
      reinterpret_cast<NtQuerySectionType>(
          ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
  DCHECK(nt_query_section_func);

  // The handle must have SECTION_QUERY access for this to succeed.
  SECTION_BASIC_INFORMATION basic_information = {};
  ULONG status =
      nt_query_section_func(handle, SectionBasicInformation, &basic_information,
                            sizeof(basic_information), nullptr);
  if (status) {
    return false;
  }

  return (basic_information.Attributes & SEC_IMAGE) != SEC_IMAGE;
}

}  // namespace

namespace base {

void SharedMemory::MappingDeleter::operator()(void* ptr) {
  UnmapViewOfFile(ptr);
}

SharedMemory::~SharedMemory() = default;

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(!mapped_file_);

  external_section_ = true;
  freezeable_ = false;  // just in case
  mapped_file_ = std::move(handle);
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle != nullptr;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() { return nullptr; }

// Wrapper around CreateFileMappingW for pagefile-backed regions. When out of
// memory, may attempt to stall and retry rather than returning immediately, in
// hopes that the page file is about to be expanded by Windows. (bug 1822383,
// bug 1716727)
//
// This method is largely a copy of the MozVirtualAlloc method from
// mozjemalloc.cpp, which implements this strategy for VirtualAlloc calls,
// except re-purposed to handle CreateFileMapping.
static HANDLE MozCreateFileMappingW(
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect,
    DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName) {
#ifdef MOZ_MEMORY
  constexpr auto IsOOMError = [] {
    return ::GetLastError() == ERROR_COMMITMENT_LIMIT;
  };

  {
    HANDLE handle = ::CreateFileMappingW(
        INVALID_HANDLE_VALUE, lpFileMappingAttributes, flProtect,
        dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    if (MOZ_LIKELY(handle)) {
      MOZ_DIAGNOSTIC_ASSERT(handle != INVALID_HANDLE_VALUE,
                            "::CreateFileMapping should return NULL, not "
                            "INVALID_HANDLE_VALUE, on failure");
      return handle;
    }

    // We can't do anything for errors other than OOM.
    if (!IsOOMError()) {
      return nullptr;
    }
  }

  // Retry as many times as desired (possibly zero).
  const mozilla::StallSpecs stallSpecs = mozilla::GetAllocatorStallSpecs();

  const auto ret =
      stallSpecs.StallAndRetry(&::Sleep, [&]() -> std::optional<HANDLE> {
        HANDLE handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE, lpFileMappingAttributes, flProtect,
            dwMaximumSizeHigh, dwMaximumSizeLow, lpName);

        if (handle) {
          MOZ_DIAGNOSTIC_ASSERT(handle != INVALID_HANDLE_VALUE,
                                "::CreateFileMapping should return NULL, not "
                                "INVALID_HANDLE_VALUE, on failure");
          return handle;
        }

        // Failure for some reason other than OOM.
        if (!IsOOMError()) {
          return nullptr;
        }

        return std::nullopt;
      });

  return ret.value_or(nullptr);
#else
  return ::CreateFileMappingW(INVALID_HANDLE_VALUE, lpFileMappingAttributes,
                              flProtect, dwMaximumSizeHigh, dwMaximumSizeLow,
                              lpName);
#endif
}

bool SharedMemory::CreateInternal(size_t size, bool freezeable) {
  DCHECK(!mapped_file_);
  read_only_ = false;

  // If the shared memory object has no DACL, any process can
  // duplicate its handles with any access rights; e.g., re-add write
  // access to a read-only handle.  To prevent that, we give it an
  // empty DACL, so that no process can do that.
  SECURITY_ATTRIBUTES sa, *psa = nullptr;
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  if (freezeable) {
    psa = &sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    if (NS_WARN_IF(!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) ||
        NS_WARN_IF(
            !InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) ||
        NS_WARN_IF(!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE))) {
      return false;
    }
  }

  mapped_file_.reset(MozCreateFileMappingW(psa, PAGE_READWRITE, 0,
                                           static_cast<DWORD>(size), nullptr));
  if (!mapped_file_) return false;

  max_size_ = size;
  freezeable_ = freezeable;
  return true;
}

bool SharedMemory::ReadOnlyCopy(SharedMemory* ro_out) {
  DCHECK(!read_only_);
  CHECK(freezeable_);

  if (ro_out == this) {
    DCHECK(!memory_);
  }

  HANDLE ro_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), mapped_file_.release(),
                         GetCurrentProcess(), &ro_handle,
                         GENERIC_READ | FILE_MAP_READ, false,
                         DUPLICATE_CLOSE_SOURCE)) {
    // DUPLICATE_CLOSE_SOURCE applies even if there is an error.
    return false;
  }

  freezeable_ = false;

  ro_out->Close();
  ro_out->mapped_file_.reset(ro_handle);
  ro_out->max_size_ = max_size_;
  ro_out->read_only_ = true;
  ro_out->freezeable_ = false;
  ro_out->external_section_ = external_section_;

  return true;
}

bool SharedMemory::Map(size_t bytes, void* fixed_address) {
  if (!mapped_file_) {
    return false;
  }

  if (external_section_ && !IsSectionSafeToMap(mapped_file_.get())) {
    return false;
  }

  void* mem = MapViewOfFileEx(
      mapped_file_.get(),
      read_only_ ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, bytes,
      fixed_address);
  if (mem) {
    MOZ_ASSERT(!fixed_address || mem == fixed_address,
               "MapViewOfFileEx returned an expected address");
    memory_.reset(mem);
    return true;
  }
  return false;
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
  return memory;
}

SharedMemoryHandle SharedMemory::CloneHandle() {
  freezeable_ = false;
  HANDLE handle = INVALID_HANDLE_VALUE;
  if (DuplicateHandle(GetCurrentProcess(), mapped_file_.get(),
                      GetCurrentProcess(), &handle, 0, false,
                      DUPLICATE_SAME_ACCESS)) {
    return SharedMemoryHandle(handle);
  }
  NS_WARNING("DuplicateHandle Failed!");
  return nullptr;
}

void SharedMemory::Close(bool unmap_view) {
  if (unmap_view) {
    Unmap();
  }

  mapped_file_ = nullptr;
}

}  // namespace base
