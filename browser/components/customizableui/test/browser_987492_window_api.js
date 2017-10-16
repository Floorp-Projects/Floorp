/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


add_task(async function testOneWindow() {
  let windows = [];
  for (let win of CustomizableUI.windows)
    windows.push(win);
  is(windows.length, 1, "Should have one customizable window");
});


add_task(async function testOpenCloseWindow() {
  let newWindow = null;
  let openListener = {
    onWindowOpened(window) {
      newWindow = window;
    }
  };
  CustomizableUI.addListener(openListener);

  {
    let win = await openAndLoadWindow(null, true);
    is(newWindow, win, "onWindowOpen event should have received expected window");
    isnot(newWindow, null, "Should have gotten onWindowOpen event");
  }

  CustomizableUI.removeListener(openListener);

  let windows = [];
  for (let win of CustomizableUI.windows)
    windows.push(win);
  is(windows.length, 2, "Should have two customizable windows");
  isnot(windows.indexOf(window), -1, "Current window should be in window collection.");
  isnot(windows.indexOf(newWindow), -1, "New window should be in window collection.");

  let closedWindow = null;
  let closeListener = {
    onWindowClosed(window) {
      closedWindow = window;
    }
  };
  CustomizableUI.addListener(closeListener);
  await promiseWindowClosed(newWindow);
  isnot(closedWindow, null, "Should have gotten onWindowClosed event");
  is(newWindow, closedWindow, "Closed window should match previously opened window");
  CustomizableUI.removeListener(closeListener);

  windows = [];
  for (let win of CustomizableUI.windows)
    windows.push(win);
  is(windows.length, 1, "Should have one customizable window");
  isnot(windows.indexOf(window), -1, "Current window should be in window collection.");
  is(windows.indexOf(closedWindow), -1, "Closed window should not be in window collection.");
});
