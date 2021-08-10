/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteType_h
#define mozilla_dom_RemoteType_h

#include "nsString.h"
#include "nsReadableUtils.h"

// These must match the similar ones in E10SUtils.jsm and ProcInfo.h.
// Process names as reported by about:memory are defined in
// ContentChild:RecvRemoteType.  Add your value there too or it will be called
// "Web Content".
#define PREALLOC_REMOTE_TYPE "prealloc"_ns
#define WEB_REMOTE_TYPE "web"_ns
#define FILE_REMOTE_TYPE "file"_ns
#define EXTENSION_REMOTE_TYPE "extension"_ns
#define PRIVILEGEDABOUT_REMOTE_TYPE "privilegedabout"_ns
#define PRIVILEGEDMOZILLA_REMOTE_TYPE "privilegedmozilla"_ns

#define DEFAULT_REMOTE_TYPE WEB_REMOTE_TYPE

// These must start with the WEB_REMOTE_TYPE above.
#define FISSION_WEB_REMOTE_TYPE "webIsolated"_ns
#define WITH_COOP_COEP_REMOTE_TYPE "webCOOP+COEP"_ns
#define WITH_COOP_COEP_REMOTE_TYPE_PREFIX "webCOOP+COEP="_ns
#define LARGE_ALLOCATION_REMOTE_TYPE "webLargeAllocation"_ns

// Remote type value used to represent being non-remote.
#define NOT_REMOTE_TYPE VoidCString()

#endif  // mozilla_dom_RemoteType_h
