/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling I/O
 */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://gre/modules/PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

this.EXPORTED_SYMBOLS = ["SessionWorker"];

this.SessionWorker = new BasePromiseWorker("resource:///modules/sessionstore/SessionWorker.js");
// As the Session Worker performs I/O, we can receive instances of
// OS.File.Error, so we need to install a decoder.
this.SessionWorker.ExceptionHandlers["OS.File.Error"] = OS.File.Error.fromMsg;

