/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Services = require("Services");

async function attachThread(target) {
  const threadOptions = {
    autoBlackBox: false,
    ignoreFrameEnvironment: true,
    pauseOnExceptions:
      Services.prefs.getBoolPref("devtools.debugger.pause-on-exceptions"),
    ignoreCaughtExceptions:
      Services.prefs.getBoolPref("devtools.debugger.ignore-caught-exceptions"),
  };

  const [, threadClient] = await target.attachThread(threadOptions);
  if (!threadClient.paused) {
    throw new Error("Thread in wrong state when starting up, should be paused.");
  }
  return threadClient;
}

module.exports = { attachThread };
