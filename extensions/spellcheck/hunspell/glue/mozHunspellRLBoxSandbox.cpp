/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozHunspellRLBoxSandbox.h"
#include "mozHunspellRLBoxGlue.h"

FileMgr::FileMgr(const char* aFilename, const char* aKey) : mFd(0) {
  // The key is not used in firefox
  mFd = moz_glue_hunspell_create_filemgr(aFilename);
}

bool FileMgr::getline(std::string& aResult) {
  char* line = nullptr;
  bool ok = moz_glue_hunspell_get_line(mFd, &line);
  if (ok && line) {
    aResult = line;
    free(line);
  }
  return ok;
}

int FileMgr::getlinenum() const { return moz_glue_hunspell_get_line_num(mFd); }

FileMgr::~FileMgr() { moz_glue_hunspell_destruct_filemgr(mFd); }

// Glue code to set global callbacks

hunspell_create_filemgr_t* moz_glue_hunspell_create_filemgr = nullptr;
hunspell_get_line_t* moz_glue_hunspell_get_line = nullptr;
hunspell_get_line_num_t* moz_glue_hunspell_get_line_num = nullptr;
hunspell_destruct_filemgr_t* moz_glue_hunspell_destruct_filemgr = nullptr;
hunspell_ToUpperCase_t* moz_hunspell_ToUpperCase = nullptr;
hunspell_ToLowerCase_t* moz_hunspell_ToLowerCase = nullptr;
hunspell_get_current_cs_t* moz_hunspell_GetCurrentCS = nullptr;

void RegisterHunspellCallbacks(
    hunspell_create_filemgr_t* aHunspellCreateFilemgr,
    hunspell_get_line_t* aHunspellGetLine,
    hunspell_get_line_num_t* aHunspellGetLine_num,
    hunspell_destruct_filemgr_t* aHunspellDestructFilemgr,
    hunspell_ToUpperCase_t* aHunspellToUpperCase,
    hunspell_ToLowerCase_t* aHunspellToLowerCase,
    hunspell_get_current_cs_t* aHunspellGetCurrentCS) {
  moz_glue_hunspell_create_filemgr = aHunspellCreateFilemgr;
  moz_glue_hunspell_get_line = aHunspellGetLine;
  moz_glue_hunspell_get_line_num = aHunspellGetLine_num;
  moz_glue_hunspell_destruct_filemgr = aHunspellDestructFilemgr;
  moz_hunspell_ToUpperCase = aHunspellToUpperCase;
  moz_hunspell_ToLowerCase = aHunspellToLowerCase;
  moz_hunspell_GetCurrentCS = aHunspellGetCurrentCS;
}
