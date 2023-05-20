/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Verify that breakpoints don't impact the browser console

"use strict";

add_task(async function () {
  await BrowserConsoleManager.toggleBrowserConsole();

  // Bug 1687657, if the thread actor is attached or set up for breakpoints,
  // the test will freeze here, by the browser console's thread actor.
  // eslint-disable-next-line no-debugger
  debugger;
});
