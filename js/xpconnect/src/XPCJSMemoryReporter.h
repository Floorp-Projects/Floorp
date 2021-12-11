/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCJSMemoryReporter_h
#define XPCJSMemoryReporter_h

class nsISupports;
class nsIHandleReportCallback;

namespace xpc {

// The key is the window ID.
typedef nsTHashMap<nsUint64HashKey, nsCString> WindowPaths;

// This is very nearly an instance of nsIMemoryReporter, but it's not,
// because it's invoked by nsWindowMemoryReporter in order to get |windowPaths|
// in CollectReports.
class JSReporter {
 public:
  static void CollectReports(WindowPaths* windowPaths,
                             WindowPaths* topWindowPaths,
                             nsIHandleReportCallback* handleReport,
                             nsISupports* data, bool anonymize);
};

}  // namespace xpc

#endif
