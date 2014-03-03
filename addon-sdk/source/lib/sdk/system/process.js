/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const {
  exit, version, stdout, stderr, platform, architecture
} = require("../system");

/**
 * Supported
 */

exports.stdout = stdout;
exports.stderr = stderr;
exports.version = version;
exports.versions = {};
exports.config = {};
exports.arch = architecture;
exports.platform = platform;
exports.exit = exit;

/**
 * Partial support
 */

// An alias to `setTimeout(fn, 0)`, which isn't the same as node's `nextTick`,
// but atleast ensures it'll occur asynchronously
exports.nextTick = (callback) => setTimeout(callback, 0);

/**
 * Unsupported
 */

exports.maxTickDepth = 1000;
exports.pid = 0;
exports.title = "";
exports.stdin = {};
exports.argv = [];
exports.execPath = "";
exports.execArgv = [];
exports.abort = function () {};
exports.chdir = function () {};
exports.cwd = function () {};
exports.env = {};
exports.getgid = function () {};
exports.setgid = function () {};
exports.getuid = function () {};
exports.setuid = function () {};
exports.getgroups = function () {};
exports.setgroups = function () {};
exports.initgroups = function () {};
exports.kill = function () {};
exports.memoryUsage = function () {};
exports.umask = function () {};
exports.uptime = function () {};
exports.hrtime = function () {};
