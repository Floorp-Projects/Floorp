// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_EXT_H_
#define CDM_CONTENT_DECRYPTION_MODULE_EXT_H_

#if defined(_WIN32)
#include <windows.h>
#endif

#include "content_decryption_module_export.h"

#if defined(_MSC_VER)
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif

namespace cdm {

#if defined(_WIN32)
typedef wchar_t FilePathCharType;
typedef HANDLE PlatformFile;
const PlatformFile kInvalidPlatformFile = INVALID_HANDLE_VALUE;
#else
typedef char FilePathCharType;
typedef int PlatformFile;
const PlatformFile kInvalidPlatformFile = -1;
#endif  // defined(_WIN32)

struct HostFile {
  HostFile(const FilePathCharType* file_path,
           PlatformFile file,
           PlatformFile sig_file)
      : file_path(file_path), file(file), sig_file(sig_file) {}

  // File that is part of the host of the CDM.
  const FilePathCharType* file_path = nullptr;
  PlatformFile file = kInvalidPlatformFile;

  // Signature file for |file|.
  PlatformFile sig_file = kInvalidPlatformFile;
};

}  // namespace cdm

extern "C" {

// Functions in this file are dynamically retrieved by their versioned function
// names. Increment the version number for any backward incompatible API
// changes.

// Verifies CDM host. All files in |host_files| are opened in read-only mode.
//
// Returns false and closes all files if there is an immediate failure.
// Otherwise returns true as soon as possible and processes the files
// asynchronously. All files MUST be closed by the CDM after this one-time
// processing is finished.
CDM_API bool VerifyCdmHost_0(const cdm::HostFile* host_files,
                             uint32_t num_files);
}

#endif  // CDM_CONTENT_DECRYPTION_MODULE_EXT_H_
