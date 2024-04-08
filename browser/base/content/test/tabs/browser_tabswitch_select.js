/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:support"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,Goodbye"
  );

  gURLBar.select();

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  let focusPromise = BrowserTestUtils.waitForEvent(
    gURLBar.inputField,
    "select",
    true
  );
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await focusPromise;

  is(gURLBar.selectionStart, 0, "url is selected");
  is(gURLBar.selectionEnd, 22, "url is selected");

  // Now check that the url bar is focused when a new tab is opened while in fullscreen.

  let fullScreenEntered = TestUtils.waitForCondition(
    () => document.documentElement.getAttribute("sizemode") == "fullscreen"
  );
  BrowserCommands.fullScreen();
  await fullScreenEntered;

  tab2.linkedBrowser.focus();

  // Open a new tab
  focusPromise = BrowserTestUtils.waitForEvent(
    gURLBar.inputField,
    "focus",
    true
  );
  EventUtils.synthesizeKey("T", { accelKey: true });
  await focusPromise;

  is(document.activeElement, gURLBar.inputField, "urlbar is focused");

  let fullScreenExited = TestUtils.waitForCondition(
    () => document.documentElement.getAttribute("sizemode") != "fullscreen"
  );
  BrowserCommands.fullScreen();
  await fullScreenExited;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
