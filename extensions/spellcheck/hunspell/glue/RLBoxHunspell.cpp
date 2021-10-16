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

using namespace rlbox;
using namespace mozilla;

// Helper function for allocating and copying nsAutoCString into sandbox
static tainted_hunspell<char*> allocStrInSandbox(
    rlbox_sandbox_hunspell& aSandbox, const nsAutoCString& str) {
  size_t size = str.Length() + 1;
  tainted_hunspell<char*> t_str = aSandbox.malloc_in_sandbox<char>(size);
  MOZ_RELEASE_ASSERT(t_str);
  rlbox::memcpy(aSandbox, t_str, str.get(), size);
  return t_str;
}

// Helper function for allocating and copying std::string into sandbox
static tainted_hunspell<char*> allocStrInSandbox(
    rlbox_sandbox_hunspell& aSandbox, const std::string& str) {
  size_t size = str.size() + 1;
  tainted_hunspell<char*> t_str = aSandbox.malloc_in_sandbox<char>(size);
  MOZ_RELEASE_ASSERT(t_str);
  rlbox::memcpy(aSandbox, t_str, str.c_str(), size);
  return t_str;
}

RLBoxHunspell::RLBoxHunspell(const nsAutoCString& affpath,
                             const nsAutoCString& dpath)
    : mHandle(nullptr) {
#ifdef MOZ_WASM_SANDBOXING_HUNSPELL
  mSandbox.create_sandbox(mozilla::ipc::GetSandboxedRLBoxPath().get());
#else
  mSandbox.create_sandbox();
#endif

  // Add the aff and dict files to allow list
  if (!affpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(affpath);
  }
  if (!dpath.IsEmpty()) {
    mozHunspellCallbacks::AllowFile(dpath);
  }

  // Register callbacks
  mCreateFilemgr =
      mSandbox.register_callback(mozHunspellCallbacks::CreateFilemgr);
  mGetLine = mSandbox.register_callback(mozHunspellCallbacks::GetLine);
  mGetLineNum = mSandbox.register_callback(mozHunspellCallbacks::GetLineNum);
  mDestructFilemgr =
      mSandbox.register_callback(mozHunspellCallbacks::DestructFilemgr);
  mHunspellToUpperCase =
      mSandbox.register_callback(mozHunspellCallbacks::ToUpperCase);
  mHunspellToLowerCase =
      mSandbox.register_callback(mozHunspellCallbacks::ToLowerCase);
  mHunspellGetCurrentCS =
      mSandbox.register_callback(mozHunspellCallbacks::GetCurrentCS);

  mSandbox.invoke_sandbox_function(RegisterHunspellCallbacks, mCreateFilemgr,
                                   mGetLine, mGetLineNum, mDestructFilemgr,
                                   mHunspellToUpperCase, mHunspellToLowerCase,
                                   mHunspellGetCurrentCS);

  // Copy the affpath and dpath into the sandbox
  tainted_hunspell<char*> t_affpath = allocStrInSandbox(mSandbox, affpath);
  tainted_hunspell<char*> t_dpath = allocStrInSandbox(mSandbox, dpath);

  // Create handle
  mHandle = mSandbox.invoke_sandbox_function(
      Hunspell_create, rlbox::sandbox_const_cast<const char*>(t_affpath),
      rlbox::sandbox_const_cast<const char*>(t_dpath));
  MOZ_RELEASE_ASSERT(mHandle);

  mSandbox.free_in_sandbox(t_dpath);
  mSandbox.free_in_sandbox(t_affpath);

  // Get dictionary encoding
  tainted_hunspell<char*> t_enc =
      mSandbox.invoke_sandbox_function(Hunspell_get_dic_encoding, mHandle);
  t_enc.copy_and_verify_string([&](std::unique_ptr<char[]> enc) {
    size_t len = std::strlen(enc.get());
    mDicEncoding = std::string(enc.get(), len);
  });
}

RLBoxHunspell::~RLBoxHunspell() {
  // Call hunspell's destroy which frees mHandle
  mSandbox.invoke_sandbox_function(Hunspell_destroy, mHandle);
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

  // Dstroy sandbox
  mSandbox.destroy_sandbox();
}

int RLBoxHunspell::spell(const std::string& stdWord) {
  // Copy word into the sandbox
  tainted_hunspell<char*> t_word = allocStrInSandbox(mSandbox, stdWord);

  // Check word
  int good = mSandbox
                 .invoke_sandbox_function(
                     Hunspell_spell, mHandle,
                     rlbox::sandbox_const_cast<const char*>(t_word))
                 .copy_and_verify([](int good) { return good; });
  mSandbox.free_in_sandbox(t_word);
  return good;
}

const std::string& RLBoxHunspell::get_dict_encoding() const {
  return mDicEncoding;
}

std::vector<std::string> RLBoxHunspell::suggest(const std::string& stdWord) {
  // Copy word into the sandbox
  tainted_hunspell<char*> t_word = allocStrInSandbox(mSandbox, stdWord);

  // Allocate suggestion list in the sandbox
  tainted_hunspell<char***> t_slst = mSandbox.malloc_in_sandbox<char**>();
  *t_slst = nullptr;

  // Get suggestions
  int nr = mSandbox
               .invoke_sandbox_function(
                   Hunspell_suggest, mHandle, t_slst,
                   rlbox::sandbox_const_cast<const char*>(t_word))
               .copy_and_verify([](int nr) {
                 MOZ_RELEASE_ASSERT(nr >= 0);
                 return nr;
               });

  // Copy suggestions from sandbox
  std::vector<std::string> suggestions;
  suggestions.reserve(nr);
  for (int i = 0; i < nr; i++) {
    tainted_hunspell<char*> t_sug = (*t_slst)[i];
    MOZ_RELEASE_ASSERT(t_sug);
    t_sug.copy_and_verify_string([&](std::unique_ptr<char[]> sug) {
      size_t len = std::strlen(sug.get());
      suggestions.push_back(std::string(sug.get(), len));
    });
  }

  mSandbox.free_in_sandbox(t_word);
  mSandbox.free_in_sandbox(t_slst);
  return suggestions;
}
