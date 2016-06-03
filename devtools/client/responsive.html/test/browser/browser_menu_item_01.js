/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test RDM menu item is checked when expected, on multiple tabs.

const TEST_URL = "data:text/html;charset=utf-8,";

const tabUtils = require("sdk/tabs/utils");
const { startup } = require("sdk/window/helpers");

const activateTab = (tab) => new Promise(resolve => {
  let { tabContainer } = tabUtils.getOwnerWindow(tab).gBrowser;

  tabContainer.addEventListener("TabSelect", function listener({type}) {
    tabContainer.removeEventListener(type, listener);
    resolve();
  });

  tabUtils.activateTab(tab);
});

const isMenuChecked = () => {
  let menu = document.getElementById("menu_responsiveUI");
  return menu.getAttribute("checked") === "true";
};

add_task(function* () {
  yield startup(window);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked by default");

  const tab = yield addTab(TEST_URL);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked for new tab");

  yield openRDM(tab);

  ok(isMenuChecked(),
    "RDM menu item is checked with RDM open");

  const tab2 = yield addTab(TEST_URL);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked for new tab");

  yield activateTab(tab);

  ok(isMenuChecked(),
    "RDM menu item is checked for the tab where RDM is open");

  yield closeRDM(tab);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked after RDM is closed");

  yield removeTab(tab);
  yield removeTab(tab2);
});
