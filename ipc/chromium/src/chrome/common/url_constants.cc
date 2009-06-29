// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

// TODO(port): Remove this header when last ifdef is removed from this file.
#include "build/build_config.h"

namespace chrome {

const char kAboutScheme[] = "about";
const char kChromeInternalScheme[] = "chrome-internal";
const char kChromeUIScheme[] = "chrome";
const char kDataScheme[] = "data";
const char kExtensionScheme[] = "chrome-extension";
const char kFileScheme[] = "file";
const char kFtpScheme[] = "ftp";
const char kGearsScheme[] = "gears";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
const char kUserScriptScheme[] = "chrome-user-script";
const char kViewCacheScheme[] = "view-cache";
const char kViewSourceScheme[] = "view-source";

const char kStandardSchemeSeparator[] = "://";

const char kAboutBlankURL[] = "about:blank";
const char kAboutCacheURL[] = "about:cache";
const char kAboutMemoryURL[] = "about:memory";

const char kChromeUIDevToolsURL[] = "chrome://devtools/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIInspectorURL[] = "chrome://inspector/";
const char kChromeUIIPCURL[] = "chrome://about/ipc";
const char kChromeUINetworkURL[] = "chrome://about/network";
const char kChromeUINewTabURL[] = "chrome://newtab";

const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDialogHost[] = "dialog";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFavIconPath[] = "favicon";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIInspectorHost[] = "inspector";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIThumbnailPath[] = "thumb";

}  // namespace chrome
