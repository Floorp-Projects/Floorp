/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EXTENSIONS_SPELLCHECK_HUNSPELL_GLUE_RLBOXHUNSPELL_H_
#define EXTENSIONS_SPELLCHECK_HUNSPELL_GLUE_RLBOXHUNSPELL_H_

#include "RLBoxHunspellTypes.h"

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"

#ifdef MOZ_WASM_SANDBOXING_HUNSPELL
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#else
// Extra configuration for no-op sandbox
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_noop_sandbox.hpp"
#endif

#include "mozilla/rlbox/rlbox.hpp"

#include <hunspell.h>
#include "mozHunspellRLBoxGlue.h"

class RLBoxHunspell {
 public:
  RLBoxHunspell(const nsAutoCString& affpath, const nsAutoCString& dpath);
  ~RLBoxHunspell();

  int spell(const std::string& stdWord);
  const std::string& get_dict_encoding() const;

  std::vector<std::string> suggest(const std::string& word);

 private:
  rlbox_sandbox_hunspell mSandbox;
  sandbox_callback_hunspell<hunspell_create_filemgr_t*> mCreateFilemgr;
  sandbox_callback_hunspell<hunspell_get_line_t*> mGetLine;
  sandbox_callback_hunspell<hunspell_get_line_num_t*> mGetLineNum;
  sandbox_callback_hunspell<hunspell_destruct_filemgr_t*> mDestructFilemgr;
  sandbox_callback_hunspell<hunspell_ToUpperCase_t*> mHunspellToUpperCase;
  sandbox_callback_hunspell<hunspell_ToLowerCase_t*> mHunspellToLowerCase;
  sandbox_callback_hunspell<hunspell_get_current_cs_t*> mHunspellGetCurrentCS;
  tainted_hunspell<Hunhandle*> mHandle;
  std::string mDicEncoding;
};

#endif
