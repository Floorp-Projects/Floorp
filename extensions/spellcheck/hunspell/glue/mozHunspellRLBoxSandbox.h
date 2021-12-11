/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozHunspellRLBoxSandbox_h
#define mozHunspellRLBoxSandbox_h

#include <string>
#include <stdint.h>

// Note: This class name and lack of namespacing terrible, but are necessary
// for Hunspell compatibility.
class FileMgr final {
 public:
  /**
   * aFilename must be a local file/jar URI for the file to load.
   *
   * aKey is the decription key for encrypted Hunzip files, and is
   * unsupported. The argument is there solely for compatibility.
   */
  explicit FileMgr(const char* aFilename, const char* aKey = nullptr);
  ~FileMgr();

  // Note: The nonstandard naming conventions of these methods are necessary for
  // Hunspell compatibility.
  bool getline(std::string& aLine);
  int getlinenum() const;

 private:
  // opaque file descriptor got from the host application
  uint32_t mFd;
};

#endif  // mozHunspellRLBoxSandbox_h
