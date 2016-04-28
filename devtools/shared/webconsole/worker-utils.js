/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// XXXworkers This file is loaded on the server side for worker debugging.
// Since the server is running in the worker thread, it doesn't
// have access to Services / Components.  This functionality
// is stubbed out to prevent errors, and will need to implemented
// for Bug 1209353.

exports.Utils = { L10n: function () {} };
exports.ConsoleServiceListener = function () {};
exports.ConsoleAPIListener = function () {};
exports.addWebConsoleCommands = function () {};
exports.ConsoleReflowListener = function () {};
exports.CONSOLE_WORKER_IDS = [];
