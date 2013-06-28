/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [];

const Cu = Components.utils;
const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;

Cu.import("resource:///modules/devtools/BuiltinCommands.jsm");
Cu.import("resource:///modules/devtools/CmdDebugger.jsm");
Cu.import("resource:///modules/devtools/CmdEdit.jsm");
Cu.import("resource:///modules/devtools/CmdInspect.jsm");
Cu.import("resource:///modules/devtools/CmdResize.jsm");
Cu.import("resource:///modules/devtools/CmdTilt.jsm");
Cu.import("resource:///modules/devtools/CmdScratchpad.jsm");

require("devtools/profiler/commands.js");
