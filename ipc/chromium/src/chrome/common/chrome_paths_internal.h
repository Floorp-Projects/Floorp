// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
#define CHROME_COMMON_CHROME_PATHS_INTERNAL_H_

class FilePath;

namespace chrome {

// Get the path to the user's data directory, regardless of whether
// DIR_USER_DATA has been overridden by a command-line option.
bool GetDefaultUserDataDirectory(FilePath* result);

// Get the path to the user's documents directory.
bool GetUserDocumentsDirectory(FilePath* result);

// Get the path to the user's downloads directory.
bool GetUserDownloadsDirectory(FilePath* result);

// The path to the user's desktop.
bool GetUserDesktop(FilePath* result);

}  // namespace chrome


#endif  // CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
