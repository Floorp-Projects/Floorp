// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H_
#define CHROME_COMMON_CHROME_CONSTANTS_H_

#include "base/file_path.h"

namespace chrome {

extern const wchar_t kBrowserProcessExecutableName[];
extern const wchar_t kBrowserProcessExecutablePath[];
extern const wchar_t kBrowserAppName[];
extern const wchar_t kMessageWindowClass[];
extern const wchar_t kExternalTabWindowClass[];
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const wchar_t kNotSignedInProfile[];
extern const wchar_t kNotSignedInID[];
extern const char    kStatsFilename[];
extern const wchar_t kBrowserResourcesDll[];
extern const FilePath::CharType kExtensionFileExtension[];

// filenames
extern const FilePath::CharType kArchivedHistoryFilename[];
extern const FilePath::CharType kCacheDirname[];
extern const FilePath::CharType kMediaCacheDirname[];
extern const FilePath::CharType kOffTheRecordMediaCacheDirname[];
extern const wchar_t kChromePluginDataDirname[];
extern const FilePath::CharType kCookieFilename[];
extern const FilePath::CharType kHistoryFilename[];
extern const FilePath::CharType kLocalStateFilename[];
extern const FilePath::CharType kPreferencesFilename[];
extern const FilePath::CharType kSafeBrowsingFilename[];
extern const FilePath::CharType kSingletonSocketFilename[];
extern const FilePath::CharType kThumbnailsFilename[];
extern const wchar_t kUserDataDirname[];
extern const FilePath::CharType kUserScriptsDirname[];
extern const FilePath::CharType kWebDataFilename[];
extern const FilePath::CharType kBookmarksFileName[];
extern const FilePath::CharType kHistoryBookmarksFileName[];
extern const FilePath::CharType kCustomDictionaryFileName[];

extern const unsigned int kMaxRendererProcessCount;
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

extern const bool kRecordModeEnabled;

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H_
