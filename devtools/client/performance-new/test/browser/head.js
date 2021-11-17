/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const BackgroundJSM = ChromeUtils.import(
  "resource://devtools/client/performance-new/popup/background.jsm.js"
);

registerCleanupFunction(() => {
  BackgroundJSM.revertRecordingSettings();
});

/**
 * Allow tests to use "require".
 */
const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);

{
  const {
    getEnvironmentVariable,
  } = require("devtools/client/performance-new/browser");

  if (getEnvironmentVariable("MOZ_PROFILER_SHUTDOWN")) {
    throw new Error(
      "These tests cannot be run with shutdown profiling as they rely on manipulating " +
        "the state of the profiler."
    );
  }

  if (getEnvironmentVariable("MOZ_PROFILER_STARTUP")) {
    throw new Error(
      "These tests cannot be run with startup profiling as they rely on manipulating " +
        "the state of the profiler."
    );
  }
}

/* import-globals-from ./helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/performance-new/test/browser/helpers.js",
  this
);
