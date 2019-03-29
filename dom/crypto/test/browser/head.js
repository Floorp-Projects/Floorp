/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let exports = this;

const scripts = [
  "util.js",
  "test-vectors.js",
];

for (let script of scripts) {
  Services.scriptloader.loadSubScript(
    `chrome://mochitests/content/browser/dom/crypto/test/browser/${script}`,
    this);
}
