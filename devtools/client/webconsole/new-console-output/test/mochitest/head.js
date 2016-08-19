/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../framework/test/shared-head.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

Services.prefs.setBoolPref("devtools.webconsole.new-frontend-enabled", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webconsole.new-frontend-enabled");
});
