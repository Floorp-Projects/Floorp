/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_SharedSection_h
#define mozilla_freestanding_SharedSection_h

#include "mozilla/DynamicBlocklist.h"
#include "mozilla/glue/SharedSection.h"
#include "mozilla/NativeNt.h"
#include "mozilla/interceptor/MMPolicies.h"

// clang-format off
#define MOZ_LITERAL_UNICODE_STRING(s)                                        \
  {                                                                          \
    /* Length of the string in bytes, less the null terminator */            \
    sizeof(s) - sizeof(wchar_t),                                             \
    /* Length of the string in bytes, including the null terminator */       \
    sizeof(s),                                                               \
    /* Pointer to the buffer */                                              \
    const_cast<wchar_t*>(s)                                                  \
  }
// clang-format on

namespace mozilla {
namespace freestanding {
class SharedSectionTestHelper;

struct DllBlockInfoComparator {
  explicit DllBlockInfoComparator(const UNICODE_STRING& aTarget)
      : mTarget(&aTarget) {}

  int operator()(const DllBlockInfo& aVal) const {
    return static_cast<int>(
        ::RtlCompareUnicodeString(mTarget, &aVal.mName, TRUE));
  }

  PCUNICODE_STRING mTarget;
};

// This class calculates RVAs of kernel32's functions and transfers them
// to a target process, where the transferred RVAs are resolved into
// function addresses so that the target process can use them after
// kernel32.dll is loaded and before IAT is resolved.
struct MOZ_TRIVIAL_CTOR_DTOR Kernel32ExportsSolver final
    : interceptor::MMPolicyInProcessEarlyStage::Kernel32Exports {
  void Init();
  bool Resolve();
};

// This class manages a section which is created in the launcher process and
// mapped in the browser process and the sandboxed processes.  The section's
// layout is represented as SharedSection::Layout.
//
// (1) Kernel32's functions required for MMPolicyInProcessEarlyStage
//     Formatted as Kernel32ExportsSolver.
//
// (2) Various flags and offsets
//
// (3) Entries in the dynamic blocklist, in DllBlockInfo format. There
//     are mNumBlockEntries of these, followed by one that has mName.Length
//     of 0. Note that the strings that contain
//     the names of the entries in the blocklist are stored concatenated
//     after the last entry. The mName pointers in each DllBlockInfo point
//     to these strings correctly in Resolve(), so clients don't need
//     to do anything special to read these strings.
//
// (4) Array of NT paths of the executable's dependent modules
//     Formatted as a null-delimited wide-character string set ending with
//     an empty string. These entries start at offset
//     mDependentModulePathArrayStart (in bytes) from the beginning
//     of the structure
//
// +--------------------------------------------------------------+
// | (1) | FlushInstructionCache                                  |
// |     | GetModuleHandleW                                       |
// |     | GetSystemInfo                                          |
// |     | VirtualProtect                                         |
// |     | State [kUninitialized|kInitialized|kResolved]          |
// +--------------------------------------------------------------+
// | (2) | (flags and offsets)                                    |
// +--------------------------------------------------------------+
// | (3) | <DllBlockInfo for first entry in dynamic blocklist>    |
// |     | <DllBlockInfo for second entry in dynamic blocklist>   |
// |     | ...                                                    |
// |     | <DllBlockInfo for last entry in dynamic blocklist>     |
// |     | <DllBlockInfo with mName.Length of 0>                  |
// |     | L"string1.dllstring2.dll...stringlast.dll"             |
// +--------------------------------------------------------------+
// | (4) | L"NT path 1"                                           |
// |     | L"NT path 2"                                           |
// |     | ...                                                    |
// |     | L""                                                    |
// +--------------------------------------------------------------+
class MOZ_TRIVIAL_CTOR_DTOR SharedSection final : public nt::SharedSection {
  struct Layout final {
    enum class State {
      kUninitialized,
      kInitialized,
      kLoadedDynamicBlocklistEntries,
      kResolved,
    } mState;

    Kernel32ExportsSolver mK32Exports;
    // 1 if the blocklist is disabled, 0 otherwise.
    // If the blocklist is disabled, the entries are still loaded to make it
    // easy for the user to remove any they don't want, but none of the DLLs
    // here are actually blocked.
    // Stored as a uint32_t for alignment reasons.
    uint32_t mBlocklistIsDisabled;
    // The offset, in bytes, from the beginning of the Layout structure to the
    // first dependent module entry.
    // When the Layout object is created, this value is 0, indicating that no
    // dependent modules have been added and it is safe to add DllBlockInfo
    // entries.
    // After this value is set to something non-0, no more DllBlockInfo entries
    // can be added.
    uint32_t mDependentModulePathArrayStart;
    // The number of blocklist entries.
    uint32_t mNumBlockEntries;
    DllBlockInfo mFirstBlockEntry[1];

    Span<DllBlockInfo> GetModulePathArray() {
      return Span<DllBlockInfo>(
          mFirstBlockEntry,
          (kSharedViewSize - (reinterpret_cast<uintptr_t>(mFirstBlockEntry) -
                              reinterpret_cast<uintptr_t>(this))) /
              sizeof(DllBlockInfo));
    }
    // Can be used to make sure we don't step past the end of the shared memory
    // section.
    static constexpr uint32_t GetMaxNumBlockEntries() {
      return (kSharedViewSize - (offsetof(Layout, mFirstBlockEntry))) /
             sizeof(DllBlockInfo);
    }
    Layout() = delete;  // disallow instantiation
    bool Resolve();
    bool IsDisabled() const;
    const DllBlockInfo* SearchBlocklist(const UNICODE_STRING& aLeafName) const;
    Span<wchar_t> GetDependentModules();
  };

  // As we define a global variable of this class and use it in our blocklist
  // which is excuted in a process's early stage.  If we have a complex dtor,
  // the static initializer tries to register that dtor with onexit() of
  // ucrtbase.dll which is not loaded yet, resulting in crash.  Thus, we have
  // a raw handle and a pointer as a static variable and manually release them
  // by calling Reset() where possible.
  static HANDLE sSectionHandle;
  static Layout* sWriteCopyView;
  static RTL_RUN_ONCE sEnsureOnce;

  // The sLock lock guarantees that while it is held, sSectionHandle will not
  // change nor get closed, sEnsureOnce will not get reinitialized, and
  // sWriteCopyView will not change nor get unmapped once initialized. We take
  // sLock on paths that could run concurrently with ConvertToReadOnly(). This
  // method is only called on the main process, and very early, so the only
  // real risk here should be threads started by third-party products reaching
  // our patched_NtMapViewOfSection (see bug 1850969).
  static nt::SRWLock sLock;

  static ULONG NTAPI EnsureWriteCopyViewOnce(PRTL_RUN_ONCE, PVOID, PVOID*);
  static Layout* EnsureWriteCopyView(bool requireKernel32Exports = false);

  static constexpr size_t kSharedViewSize = 0x1000;

  // For test use only
  friend class SharedSectionTestHelper;

 public:
  // Replace |sSectionHandle| with a given handle.
  static void Reset(HANDLE aNewSectionObject = sSectionHandle);

  static inline nt::AutoSharedLock AutoNoReset() {
    return nt::AutoSharedLock{sLock};
  }

  // Replace |sSectionHandle| with a new readonly handle.
  static void ConvertToReadOnly();

  // Create a new writable section and initialize the Kernel32ExportsSolver
  // part.
  static LauncherVoidResult Init();

  // Append a new string to the |sSectionHandle|
  static LauncherVoidResult AddDependentModule(PCUNICODE_STRING aNtPath);
  static LauncherVoidResult SetBlocklist(const DynamicBlockList& aBlocklist,
                                         bool isDisabled);

  // Map |sSectionHandle| to a copy-on-write page and return a writable pointer
  // to each structure, or null if Layout failed to resolve exports.
  Kernel32ExportsSolver* GetKernel32Exports();
  Span<const wchar_t> GetDependentModules() final override;
  Span<const DllBlockInfo> GetDynamicBlocklist() final override;

  static bool IsDisabled();
  static const DllBlockInfo* SearchBlocklist(const UNICODE_STRING& aLeafName);

  // Transfer |sSectionHandle| to a process associated with |aTransferMgr|.
  static LauncherVoidResult TransferHandle(
      nt::CrossExecTransferManager& aTransferMgr, DWORD aDesiredAccess,
      HANDLE* aDestinationAddress = &sSectionHandle);
};

extern SharedSection gSharedSection;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_SharedSection_h
