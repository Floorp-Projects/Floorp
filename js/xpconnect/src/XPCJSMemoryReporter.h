/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCJSMemoryReporter_h
#define XPCJSMemoryReporter_h

class nsISupports;
class nsIMemoryReporterCallback;

namespace xpc {

// The key is the window ID.
typedef nsDataHashtable<nsUint64HashKey, nsCString> WindowPaths;

// This is very nearly an instance of nsIMemoryReporter, but it's not,
// because it's invoked by nsWindowMemoryReporter in order to get |windowPaths|
// in CollectReports.
class JSReporter
{
public:
    static void CollectReports(WindowPaths* windowPaths,
                               WindowPaths* topWindowPaths,
                               nsIMemoryReporterCallback* handleReport,
                               nsISupports* data,
                               bool anonymize);
};

} // namespace xpc

#endif
