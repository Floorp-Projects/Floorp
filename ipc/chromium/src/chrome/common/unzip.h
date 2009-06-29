// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_UNZIP_H_
#define CHROME_COMMON_UNZIP_H_

#include <vector>

#include "base/file_path.h"

// Unzip the contents of zip_file into dest_dir.  The complete paths of all
// created files and directories are added to files if it is non-NULL.
// Returns true on success.  Does not clean up dest_dir on failure.
// Does not support encrypted or password protected zip files.
bool Unzip(const FilePath& zip_file, const FilePath& dest_dir,
           std::vector<FilePath>* files);

#endif  // CHROME_COMMON_UNZIP_H_
