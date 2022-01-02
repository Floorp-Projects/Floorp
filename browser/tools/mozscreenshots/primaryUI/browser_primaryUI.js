/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../head.js */

"use strict";

add_task(async function capture() {
  if (!shouldCapture()) {
    return;
  }

  let sets = [
    "TabsInTitlebar",
    "Tabs",
    "WindowSize",
    "Toolbars",
    "LightweightThemes",
    "UIDensities",
  ];
  await TestRunner.start(sets, "primaryUI");
});
