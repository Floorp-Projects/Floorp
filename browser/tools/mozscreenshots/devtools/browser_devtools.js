/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../head.js */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { TargetFactory } = require("devtools/client/framework/target");
const { gDevTools } = require("devtools/client/framework/devtools");

add_task(async function capture() {
  if (!shouldCapture()) {
    return;
  }
  let sets = ["DevTools"];

  await TestRunner.start(sets, "devtools");

  let target = await TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);
});
