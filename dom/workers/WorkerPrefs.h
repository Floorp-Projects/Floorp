/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the list of the preferences that are exposed to workers.
// The format is as follows:
//
//   WORKER_SIMPLE_PREF("foo.bar", FooBar, FOO_BAR, UpdaterFunction)
//
//   * First argument is the name of the pref.
//   * The name of the getter function.  This defines a FindName()
//     function that returns the value of the pref on WorkerPrivate.
//   * The macro version of the name.  This defines a WORKERPREF_FOO_BAR
//     macro in Workers.h.
//   * The name of the function that updates the new value of a pref.
//
//   WORKER_PREF("foo.bar", UpdaterFunction)
//
//   * First argument is the name of the pref.
//   * The name of the function that updates the new value of a pref.

WORKER_SIMPLE_PREF("dom.fetchObserver.enabled", FetchObserverEnabled, FETCHOBSERVER_ENABLED)
WORKER_SIMPLE_PREF("privacy.resistFingerprinting", ResistFingerprintingEnabled, RESISTFINGERPRINTING_ENABLED)
WORKER_SIMPLE_PREF("devtools.enabled", DevToolsEnabled, DEVTOOLS_ENABLED)
WORKER_PREF("intl.accept_languages", PrefLanguagesChanged)
WORKER_PREF("general.appname.override", AppNameOverrideChanged)
WORKER_PREF("general.appversion.override", AppVersionOverrideChanged)
WORKER_PREF("general.platform.override", PlatformOverrideChanged)
#ifdef JS_GC_ZEAL
WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions)
#endif
