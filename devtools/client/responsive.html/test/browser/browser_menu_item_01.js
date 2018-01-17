/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test RDM menu item is checked when expected, on multiple tabs.

const TEST_URL = "data:text/html;charset=utf-8,";

const { startup } = require("devtools/client/responsive.html/utils/window");

const activateTab = (tab) => new Promise(resolve => {
  let { gBrowser } = tab.ownerGlobal;
  let { tabContainer } = gBrowser;

  tabContainer.addEventListener("TabSelect", function listener({type}) {
    tabContainer.removeEventListener(type, listener);
    resolve();
  });

  gBrowser.selectedTab = tab;
});

const isMenuChecked = () => {
  let menu = document.getElementById("menu_responsiveUI");
  return menu.getAttribute("checked") === "true";
};

add_task(async function () {
  await startup(window);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked by default");

  const tab = await addTab(TEST_URL);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked for new tab");

  await openRDM(tab);

  ok(isMenuChecked(),
    "RDM menu item is checked with RDM open");

  const tab2 = await addTab(TEST_URL);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked for new tab");

  await activateTab(tab);

  ok(isMenuChecked(),
    "RDM menu item is checked for the tab where RDM is open");

  await closeRDM(tab);

  ok(!isMenuChecked(),
    "RDM menu item is unchecked after RDM is closed");

  await removeTab(tab);
  await removeTab(tab2);
});
