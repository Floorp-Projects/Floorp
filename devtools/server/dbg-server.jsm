/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Loads the remote debugging protocol code into a sandbox, in order to
 * shield it from the debuggee. This way, when debugging chrome globals,
 * debugger and debuggee will be in separate compartments.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

this.EXPORTED_SYMBOLS = ["DebuggerServer", "ActorPool"];

var server = require("devtools/server/main");

this.DebuggerServer = server.DebuggerServer;
this.ActorPool = server.ActorPool;
this.OriginalLocation = server.OriginalLocation;
