// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

namespace chrome {

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kAboutScheme[];
extern const char kChromeInternalScheme[];  // Used for new tab page.
extern const char kChromeUIScheme[];  // The scheme used for DOMUIContentses.
extern const char kDataScheme[];
extern const char kExtensionScheme[];
extern const char kFileScheme[];
extern const char kFtpScheme[];
extern const char kGearsScheme[];
extern const char kHttpScheme[];
extern const char kHttpsScheme[];
extern const char kJavaScriptScheme[];
extern const char kMailToScheme[];
extern const char kUserScriptScheme[];
extern const char kViewCacheScheme[];
extern const char kViewSourceScheme[];

// Used to separate a standard scheme and the hostname: "://".
extern const char kStandardSchemeSeparator[];

// About URLs (including schmes).
extern const char kAboutBlankURL[];
extern const char kAboutCacheURL[];
extern const char kAboutMemoryURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUIInspectorURL[];
extern const char kChromeUINetworkURL[];
extern const char kChromeUINewTabURL[];

// chrome components of URLs. Should be kept in sync with the full URLs
// above.
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDialogHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFavIconPath[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIInspectorHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIThumbnailPath[];

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
