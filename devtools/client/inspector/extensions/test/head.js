/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */

"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

// Import the inspector extensions test helpers (shared between the tests that live
// in the current devtools test directory and the devtools sidebar tests that live
// in browser/components/extensions/test/browser).
Services.scriptloader.loadSubScript(
  new URL("head_devtools_inspector_sidebar.js", gTestPath).href,
  this
);
