/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

// Make sure only the flexbox layout accordion is opened, and the others are closed.
Services.prefs.setBoolPref("devtools.layout.flexbox.opened", true);
Services.prefs.setBoolPref("devtools.layout.boxmodel.opened", false);
Services.prefs.setBoolPref("devtools.layout.grid.opened", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.layout.flexbox.opened");
  Services.prefs.clearUserPref("devtools.layout.boxmodel.opened");
  Services.prefs.clearUserPref("devtools.layout.grid.opened");
});
