/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test RDM menu item is checked when expected, on multiple tabs.

const TEST_URL = "data:text/html;charset=utf-8,";

const {
  startup,
} = require("resource://devtools/client/responsive/utils/window.js");

const activateTab = tab =>
  new Promise(resolve => {
    const { gBrowser } = tab.ownerGlobal;
    const { tabContainer } = gBrowser;

    tabContainer.addEventListener("TabSelect", function listener({ type }) {
      tabContainer.removeEventListener(type, listener);
      resolve();
    });

    gBrowser.selectedTab = tab;
  });

const isMenuChecked = () => {
  const menu = document.getElementById("menu_responsiveUI");
  return menu.getAttribute("checked") === "true";
};

add_task(async function () {
  await startup(window);

  ok(!isMenuChecked(), "RDM menu item is unchecked by default");
});

let tab2;

addRDMTaskWithPreAndPost(
  TEST_URL,
  function pre_task() {
    ok(!isMenuChecked(), "RDM menu item is unchecked for new tab");
  },
  async function task({ browser }) {
    ok(isMenuChecked(), "RDM menu item is checked with RDM open");

    tab2 = await addTab(TEST_URL);

    ok(!isMenuChecked(), "RDM menu item is unchecked for new tab");

    const tab = gBrowser.getTabForBrowser(browser);
    await activateTab(tab);

    ok(
      isMenuChecked(),
      "RDM menu item is checked for the tab where RDM is open"
    );
  },
  function post_task() {
    ok(!isMenuChecked(), "RDM menu item is unchecked after RDM is closed");
  }
);

add_task(async function () {
  await removeTab(tab2);
});
