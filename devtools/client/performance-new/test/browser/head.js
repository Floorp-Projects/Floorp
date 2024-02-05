/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const BackgroundJSM = ChromeUtils.importESModule(
  "resource://devtools/client/performance-new/shared/background.sys.mjs"
);

registerCleanupFunction(() => {
  BackgroundJSM.revertRecordingSettings();
});

/**
 * Allow tests to use "require".
 */
const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);

{
  if (Services.env.get("MOZ_PROFILER_SHUTDOWN")) {
    throw new Error(
      "These tests cannot be run with shutdown profiling as they rely on manipulating " +
        "the state of the profiler."
    );
  }

  if (Services.env.get("MOZ_PROFILER_STARTUP")) {
    throw new Error(
      "These tests cannot be run with startup profiling as they rely on manipulating " +
        "the state of the profiler."
    );
  }
}

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/performance-new/test/browser/helpers.js",
  this
);
