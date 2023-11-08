/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { OnRefTestLoad, OnRefTestUnload } = ChromeUtils.importESModule(
  "resource://reftest/reftest.sys.mjs"
);

function OnLoad() {
  OnRefTestLoad(window);
}

function OnUnload() {
  OnRefTestUnload();
}
