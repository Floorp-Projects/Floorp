/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "RLBoxHunspell.h"
#include "mozHunspellRLBoxGlue.h"
#include "mozHunspellRLBoxHost.h"
#include "nsThread.h"

using namespace rlbox;
using namespace mozilla;

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
RLBoxHunspell* RLBoxHunspell::Create(const nsCString& affpath,
                                     const nsCString& dpath) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  mozilla::UniquePtr<rlbox_sandbox_hunspell> sandbox(
      new rlbox_sandbox_hunspell());

#if defined(MOZ_WASM_SANDBOXING_HUNSPELL) && !defined(HAVE_64BIT_BUILD)
  // By default, the rlbox sandbox size is smaller on 32-bit builds than the max
  // 4GB. We may need to ask for a larger sandbox size for hunspell to
  // spellcheck in some locales See Bug 1739669 for more details

  // We first get the size of the dictionary. This is actually the first read we
  // try on dpath and it might fail for whatever filesystem reasons (invalid
  // path, unaccessible, ...).
  Result<int64_t, nsresult> dictSizeResult =
      mozHunspellFileMgrHost::GetSize(dpath);
  NS_ENSURE_TRUE(dictSizeResult.isOk(), nullptr);

  int64_t dictSize = dictSizeResult.unwrap();
  NS_ENSURE_TRUE(dictSize >= 0, nullptr);

  // Next, we compute the expected memory needed for hunspell spell checking.
  // This will vary based on the size of the dictionary file, which varies by
  // locale — so we size the sandbox by multiplying the file size by 4.8. This
  // allows the 1.5MB en_US dictionary to fit in an 8MB sandbox. See bug 1739669
  // and bug 1739761 for the analysis behind this.
  const uint64_t expectedMaxMemory = static_cast<uint64_t>(4.8 * dictSize);

  // Get a capacity of at least the expected size
  const w2c_mem_capacity capacity = get_valid_wasm2c_memory_capacity(
      expectedMaxMemory, true /* wasm's 32-bit memory */);

  bool success =
      sandbox->create_sandbox(/* shouldAbortOnFailure = */ false, &capacity);
#elif defined(MOZ_WASM_SANDBOXING_HUNSPELL)
  bool success = sandbox->create_sandbox(/* shouldAbortOnFailure = */ false);
#else
  sandbox->create_sandbox();
  const bool success = true;
#endif

  NS_ENSURE_TRUE(success, nullptr);

  mozilla::UniquePtr<rlbox_sandbox_hunspell, RLBoxDeleter> sandbox_initialized(
      sandbox.release());

  // Add the aff and dict files to allow list
  if (!affpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(affpath);
  }
  if (!dpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(dpath);
  }

  // TODO Bug 1788857: Verify error handling in case of inaccessible file
  return new RLBoxHunspell(std::move(sandbox_initialized), affpath, dpath);
}

RLBoxHunspell::RLBoxHunspell(
    mozilla::UniquePtr<rlbox_sandbox_hunspell, RLBoxDeleter> aSandbox,
    const nsCString& affpath, const nsCString& dpath)
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
  tainted_hunspell<char*> t_affpath =
      allocStrInSandbox(*mSandbox, affpath.get());
  MOZ_RELEASE_ASSERT(t_affpath);

  tainted_hunspell<char*> t_dpath = allocStrInSandbox(*mSandbox, dpath.get());
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

// Invoking hunspell with words larger than a certain size will cause the
// Hunspell sandbox to run out of memory. So we pick an arbitrary limit of
// 200000 here to ensure this doesn't happen.
static const size_t gWordSizeLimit = 200000;

int RLBoxHunspell::spell(const std::string& stdWord) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  const int ok = 1;

  if (stdWord.length() >= gWordSizeLimit) {
    // Fail gracefully assuming the word is spelt correctly
    return ok;
  }

  // Copy word into the sandbox
  tainted_hunspell<char*> t_word = allocStrInSandbox(*mSandbox, stdWord);
  if (!t_word) {
    // Ran out of memory in the hunspell sandbox
    // Fail gracefully assuming the word is spelt correctly
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

  if (stdWord.length() >= gWordSizeLimit) {
    return {};
  }

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
