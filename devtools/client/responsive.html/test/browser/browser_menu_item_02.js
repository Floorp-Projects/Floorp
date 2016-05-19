/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test RDM menu item is checked when expected, on multiple windows.

const TEST_URL = "data:text/html;charset=utf-8,";

const { getActiveTab } = require("sdk/tabs/utils");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { open, close, startup } = require("sdk/window/helpers");
const { partial } = require("sdk/lang/functional");

const openBrowserWindow = partial(open, null, { features: { toolbar: true } });

const isMenuCheckedFor = ({document}) => {
  let menu = document.getElementById("menu_responsiveUI");
  return menu.getAttribute("checked") === "true";
};

add_task(function* () {
  const window1 = yield openBrowserWindow();

  yield startup(window1);

  yield BrowserTestUtils.openNewForegroundTab(window1.gBrowser, TEST_URL);

  const tab1 = getActiveTab(window1);

  is(window1, getMostRecentBrowserWindow(),
    "The new window is the active one");

  ok(!isMenuCheckedFor(window1),
    "RDM menu item is unchecked by default");

  yield openRDM(tab1);

  ok(isMenuCheckedFor(window1),
    "RDM menu item is checked with RDM open");

  yield close(window1);

  is(window, getMostRecentBrowserWindow(),
    "The original window is the active one");

  ok(!isMenuCheckedFor(window),
    "RDM menu item is unchecked");
});
