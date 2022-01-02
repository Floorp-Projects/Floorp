/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#ifdef MOZ_WASM_SANDBOXING_HUNSPELL
#  include "mozilla/ipc/LibrarySandboxPreload.h"
#endif
#include "RLBoxHunspell.h"
#include "mozHunspellRLBoxGlue.h"
#include "mozHunspellRLBoxHost.h"
#include "nsThread.h"

using namespace rlbox;
using namespace mozilla;

// Helper function for allocating and copying nsAutoCString into sandbox
static tainted_hunspell<char*> allocStrInSandbox(
    rlbox_sandbox_hunspell& aSandbox, const nsAutoCString& str) {
  size_t size = str.Length() + 1;
  tainted_hunspell<char*> t_str = aSandbox.malloc_in_sandbox<char>(size);
  if (t_str) {
    rlbox::memcpy(aSandbox, t_str, str.get(), size);
  }
  return t_str;
}

// Helper function for allocating and copying std::string into sandbox
static tainted_hunspell<char*> allocStrInSandbox(
    rlbox_sandbox_hunspell& aSandbox, const std::string& str) {
  size_t size = str.size() + 1;
  tainted_hunspell<char*> t_str = aSandbox.malloc_in_sandbox<char>(size);
  if (t_str) {
    rlbox::memcpy(aSandbox, t_str, str.c_str(), size);
  }
  return t_str;
}

/* static */
RLBoxHunspell* RLBoxHunspell::Create(const nsAutoCString& affpath,
                                     const nsAutoCString& dpath) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  mozilla::UniquePtr<rlbox_sandbox_hunspell, RLBoxDeleter> sandbox(
      new rlbox_sandbox_hunspell());

#if defined(MOZ_WASM_SANDBOXING_HUNSPELL) && !defined(HAVE_64BIT_BUILD)
  // By default, the rlbox sandbox size is smaller on 32-bit builds than the max
  // 4GB We may need to ask for a larger sandbox size for hunspell to spellcheck
  // in some locales See Bug 1739669 for more details

  const uint64_t defaultMaxSizeForSandbox =
      wasm_rt_get_default_max_linear_memory_size();

  // We first get the size of the dictionary
  Result<int64_t, nsresult> dictSizeResult =
      mozHunspellFileMgrHost::GetSize(dpath);
  MOZ_RELEASE_ASSERT(dictSizeResult.isOk());

  int64_t dictSize = dictSizeResult.unwrap();
  MOZ_RELEASE_ASSERT(dictSize >= 0);

  // Next, we compute the expected memory needed for hunspell spell checking.
  // This will vary based on the size of the dictionary file, which varies by
  // locale — so we size the sandbox by multiplying the file size by 4.8. This
  // allows the 1.5MB en_US dictionary to fit in an 8MB sandbox. See bug 1739669
  // and bug 1739761 for the analysis behind this.
  const uint64_t expectedMaxMemory = static_cast<uint64_t>(4.8 * dictSize);

  // If we expect a higher memory usage, override the defaults
  // else stick with the defaults for the sandbox
  const uint64_t selectedMaxMemory =
      std::max(expectedMaxMemory, defaultMaxSizeForSandbox);

  bool success = sandbox->create_sandbox(/* shouldAbortOnFailure = */ false,
                                         selectedMaxMemory);
#elif defined(MOZ_WASM_SANDBOXING_HUNSPELL)
  bool success = sandbox->create_sandbox(/* shouldAbortOnFailure = */ false);
#else
  sandbox->create_sandbox();
  const bool success = true;
#endif

  NS_ENSURE_TRUE(success, nullptr);

  // Add the aff and dict files to allow list
  if (!affpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(affpath);
  }
  if (!dpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(dpath);
  }

  return new RLBoxHunspell(std::move(sandbox), affpath, dpath);
}

RLBoxHunspell::RLBoxHunspell(
    mozilla::UniquePtr<rlbox_sandbox_hunspell, RLBoxDeleter> aSandbox,
    const nsAutoCString& affpath, const nsAutoCString& dpath)
    : mSandbox(std::move(aSandbox)), mHandle(nullptr) {
  // Register callbacks
  mCreateFilemgr =
      mSandbox->register_callback(mozHunspellCallbacks::CreateFilemgr);
  mGetLine = mSandbox->register_callback(mozHunspellCallbacks::GetLine);
  mGetLineNum = mSandbox->register_callback(mozHunspellCallbacks::GetLineNum);
  mDestructFilemgr =
      mSandbox->register_callback(mozHunspellCallbacks::DestructFilemgr);
  mHunspellToUpperCase =
      mSandbox->register_callback(mozHunspellCallbacks::ToUpperCase);
  mHunspellToLowerCase =
      mSandbox->register_callback(mozHunspellCallbacks::ToLowerCase);
  mHunspellGetCurrentCS =
      mSandbox->register_callback(mozHunspellCallbacks::GetCurrentCS);

  mSandbox->invoke_sandbox_function(RegisterHunspellCallbacks, mCreateFilemgr,
                                    mGetLine, mGetLineNum, mDestructFilemgr,
                                    mHunspellToUpperCase, mHunspellToLowerCase,
                                    mHunspellGetCurrentCS);

  // Copy the affpath and dpath into the sandbox
  // These allocations should definitely succeed as these are first allocations
  // inside the sandbox.
  tainted_hunspell<char*> t_affpath = allocStrInSandbox(*mSandbox, affpath);
  MOZ_RELEASE_ASSERT(t_affpath);

  tainted_hunspell<char*> t_dpath = allocStrInSandbox(*mSandbox, dpath);
  MOZ_RELEASE_ASSERT(t_dpath);

  // Create handle
  mHandle = mSandbox->invoke_sandbox_function(
      Hunspell_create, rlbox::sandbox_const_cast<const char*>(t_affpath),
      rlbox::sandbox_const_cast<const char*>(t_dpath));
  MOZ_RELEASE_ASSERT(mHandle);

  mSandbox->free_in_sandbox(t_dpath);
  mSandbox->free_in_sandbox(t_affpath);

  // Get dictionary encoding
  tainted_hunspell<char*> t_enc =
      mSandbox->invoke_sandbox_function(Hunspell_get_dic_encoding, mHandle);
  t_enc.copy_and_verify_string([&](std::unique_ptr<char[]> enc) {
    size_t len = std::strlen(enc.get());
    mDicEncoding = std::string(enc.get(), len);
  });
}

RLBoxHunspell::~RLBoxHunspell() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  // Call hunspell's destroy which frees mHandle
  mSandbox->invoke_sandbox_function(Hunspell_destroy, mHandle);
  mHandle = nullptr;

  // Unregister callbacks
  mDestructFilemgr.unregister();
  mGetLineNum.unregister();
  mGetLine.unregister();
  mCreateFilemgr.unregister();
  mHunspellToUpperCase.unregister();
  mHunspellToLowerCase.unregister();
  mHunspellGetCurrentCS.unregister();

  // Clear any callback data and allow list
  mozHunspellCallbacks::Clear();
}

int RLBoxHunspell::spell(const std::string& stdWord) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  // Copy word into the sandbox
  tainted_hunspell<char*> t_word = allocStrInSandbox(*mSandbox, stdWord);
  if (!t_word) {
    // Ran out of memory in the hunspell sandbox
    // Fail gracefully assuming the word is spelt correctly
    const int ok = 1;
    return ok;
  }

  // Check word
  int good = mSandbox
                 ->invoke_sandbox_function(
                     Hunspell_spell, mHandle,
                     rlbox::sandbox_const_cast<const char*>(t_word))
                 .copy_and_verify([](int good) { return good; });
  mSandbox->free_in_sandbox(t_word);
  return good;
}

const std::string& RLBoxHunspell::get_dict_encoding() const {
  return mDicEncoding;
}

// This function fails gracefully - if we run out of memory in the hunspell
// sandbox, we return empty suggestion list
std::vector<std::string> RLBoxHunspell::suggest(const std::string& stdWord) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  // Copy word into the sandbox
  tainted_hunspell<char*> t_word = allocStrInSandbox(*mSandbox, stdWord);
  if (!t_word) {
    return {};
  }

  // Allocate suggestion list in the sandbox
  tainted_hunspell<char***> t_slst = mSandbox->malloc_in_sandbox<char**>();
  if (!t_slst) {
    // Free the earlier allocation
    mSandbox->free_in_sandbox(t_word);
    return {};
  }

  *t_slst = nullptr;

  // Get suggestions
  int nr = mSandbox
               ->invoke_sandbox_function(
                   Hunspell_suggest, mHandle, t_slst,
                   rlbox::sandbox_const_cast<const char*>(t_word))
               .copy_and_verify([](int nr) {
                 MOZ_RELEASE_ASSERT(nr >= 0);
                 return nr;
               });

  tainted_hunspell<char**> t_slst_ref = *t_slst;

  std::vector<std::string> suggestions;
  if (nr > 0 && t_slst_ref != nullptr) {
    // Copy suggestions from sandbox
    suggestions.reserve(nr);

    for (int i = 0; i < nr; i++) {
      tainted_hunspell<char*> t_sug = t_slst_ref[i];

      if (t_sug) {
        t_sug.copy_and_verify_string(
            [&](std::string sug) { suggestions.push_back(std::move(sug)); });
        // free the suggestion string allocated by the sandboxed hunspell
        mSandbox->free_in_sandbox(t_sug);
      }
    }

    // free the suggestion list allocated by the sandboxed hunspell
    mSandbox->free_in_sandbox(t_slst_ref);
  }

  mSandbox->free_in_sandbox(t_word);
  mSandbox->free_in_sandbox(t_slst);
  return suggestions;
}
