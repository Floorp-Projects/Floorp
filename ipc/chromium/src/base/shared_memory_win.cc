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
#include "mozilla/WindowsVersion.h"
#include "nsDebug.h"
#include "nsString.h"

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

SharedMemory::SharedMemory()
    : memory_(nullptr),
      max_size_(0),
      mapped_file_(nullptr),
      external_section_(false),
      read_only_(false),
      freezeable_(false) {}

SharedMemory::SharedMemory(SharedMemory&& other) {
  memory_ = other.memory_;
  max_size_ = other.max_size_;
  mapped_file_ = std::move(other.mapped_file_);
  external_section_ = other.external_section_;
  read_only_ = other.read_only_;
  freezeable_ = other.freezeable_;

  other.memory_ = nullptr;
}

SharedMemory::~SharedMemory() {
  external_section_ = true;
  Close();
}

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(!mapped_file_);

  external_section_ = true;
  freezeable_ = false;  // just in case
  mapped_file_.reset(handle);
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle != nullptr;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() { return nullptr; }

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
  nsAutoStringN<sizeof("MozSharedMem_") + 16 * 4> name;

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

    // Older versions of Windows will silently ignore the security
    // attributes unless the object has a name.
    if (!mozilla::IsWin8Point1OrLater()) {
      name.AssignLiteral("MozSharedMem_");
      for (size_t i = 0; i < 4; ++i) {
        mozilla::Maybe<uint64_t> randomNum = mozilla::RandomUint64();
        if (NS_WARN_IF(randomNum.isNothing())) {
          return false;
        }
        name.AppendPrintf("%016llx", *randomNum);
      }
    }
  }

  mapped_file_.reset(CreateFileMapping(
      INVALID_HANDLE_VALUE, psa, PAGE_READWRITE, 0, static_cast<DWORD>(size),
      name.IsEmpty() ? nullptr : name.get()));
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

  memory_ = MapViewOfFileEx(
      mapped_file_.get(),
      read_only_ ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, bytes,
      fixed_address);
  if (memory_ != NULL) {
    MOZ_ASSERT(!fixed_address || memory_ == fixed_address,
               "MapViewOfFileEx returned an expected address");
    return true;
  }
  return false;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL) return false;

  UnmapViewOfFile(memory_);
  memory_ = NULL;
  return true;
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
  return memory;
}

bool SharedMemory::ShareToProcessCommon(ProcessId processId,
                                        SharedMemoryHandle* new_handle,
                                        bool close_self) {
  freezeable_ = false;
  *new_handle = 0;
  DWORD access = FILE_MAP_READ | SECTION_QUERY;
  DWORD options = 0;
  HANDLE mapped_file;
  HANDLE result;
  if (!read_only_) {
    access |= FILE_MAP_WRITE;
  }
  if (close_self) {
    // DUPLICATE_CLOSE_SOURCE causes DuplicateHandle to close mapped_file.
    mapped_file = mapped_file_.release();
    options = DUPLICATE_CLOSE_SOURCE;
    Unmap();
  } else {
    mapped_file = mapped_file_.get();
  }

  if (processId == GetCurrentProcId() && close_self) {
    *new_handle = mapped_file;
    return true;
  }

  if (!mozilla::ipc::DuplicateHandle(mapped_file, processId, &result, access,
                                     options)) {
    return false;
  }

  *new_handle = result;
  return true;
}

void SharedMemory::Close(bool unmap_view) {
  if (unmap_view) {
    Unmap();
  }

  mapped_file_ = nullptr;
}

}  // namespace base
