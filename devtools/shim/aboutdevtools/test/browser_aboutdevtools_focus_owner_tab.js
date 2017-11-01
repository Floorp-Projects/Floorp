/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */

/**
 * When closing about:devtools, test that the tab where the user triggered about:devtools
 * is selected again.
 */
add_task(async function () {
  await pushPref("devtools.enabled", false);

  info("Add an about:blank tab");
  let tab1 = await addTab("data:text/html;charset=utf-8,tab1");
  let tab2 = await addTab("data:text/html;charset=utf-8,tab2");
  ok(tab1 === gBrowser.tabs[1], "tab1 is the second tab in the current browser window");

  info("Select the first tab");
  gBrowser.selectedTab = tab1;

  synthesizeToggleToolboxKey();

  info("Wait for the about:devtools tab to be selected");
  await waitUntil(() => isAboutDevtoolsTab(gBrowser.selectedTab));
  info("about:devtools was opened as expected.");

  let aboutDevtoolsTab = gBrowser.selectedTab;
  ok(aboutDevtoolsTab === gBrowser.tabs[2],
    "about:devtools was opened next to its owner tab");

  info("Move the owner tab to the end of the tabs array.");
  gBrowser.moveTabTo(tab1, gBrowser.tabs.length - 1);
  await removeTab(aboutDevtoolsTab);

  await waitUntil(() => tab1 == gBrowser.selectedTab);
  info("The correct tab was selected after closing about:devtools.");

  await removeTab(tab1);
  await removeTab(tab2);
});

/**
 * When closing about:devtools, test that the current tab is not updated if
 * about:devtools was not the selectedTab.
 */
add_task(async function () {
  await pushPref("devtools.enabled", false);

  info("Add an about:blank tab");
  let tab1 = await addTab("data:text/html;charset=utf-8,tab1");
  let tab2 = await addTab("data:text/html;charset=utf-8,tab2");
  ok(tab1 === gBrowser.tabs[1], "tab1 is the second tab in the current browser window");

  info("Select the first tab");
  gBrowser.selectedTab = tab1;

  synthesizeToggleToolboxKey();

  info("Wait for the about:devtools tab to be selected");
  await waitUntil(() => isAboutDevtoolsTab(gBrowser.selectedTab));
  info("about:devtools was opened as expected.");

  let aboutDevtoolsTab = gBrowser.selectedTab;
  ok(aboutDevtoolsTab === gBrowser.tabs[2],
    "about:devtools was opened next to its owner tab");

  info("Select the second tab");
  gBrowser.selectedTab = tab2;

  let aboutDevtoolsDocument = aboutDevtoolsTab.linkedBrowser.contentDocument;
  await waitUntil(() => aboutDevtoolsDocument.visibilityState === "hidden");

  await removeTab(aboutDevtoolsTab);

  ok(tab2 == gBrowser.selectedTab,
    "Tab 2 should still be selected.");

  await removeTab(tab1);
  await removeTab(tab2);
});
