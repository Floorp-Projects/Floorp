 /* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
/* import-globals-from ../../../inspector/rules/test/head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */
/* import-globals-from ../../../shared/test/shared-redux-head.js */
"use strict";

// Load the Rule view's test/head.js to make use of its helpers.
// It loads inspector/test/head.js which itself loads inspector/test/shared-head.js
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/rules/test/head.js",
  this);

// Load the shared Redux helpers.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Ensure the Changes panel is enabled before running the tests.
Services.prefs.setBoolPref("devtools.inspector.changes.enabled", true);
// Ensure the three-pane mode is enabled before running the tests.
Services.prefs.setBoolPref("devtools.inspector.three-pane-enabled", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.changes.enabled");
  Services.prefs.clearUserPref("devtools.inspector.three-pane-enabled");
});
