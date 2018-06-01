/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */

/**
 * Test that only one tab of about:devtools is used for a given window.
 */
add_task(async function() {
  await pushPref("devtools.enabled", false);

  info("Add an about:blank tab");
  const tab = await addTab("about:blank");

  synthesizeToggleToolboxKey();

  info("Wait for the about:devtools tab to be selected");
  await waitUntil(() => isAboutDevtoolsTab(gBrowser.selectedTab));

  // Keep a reference on this tab to assert it later on.
  const aboutDevtoolsTab = gBrowser.selectedTab;

  info("Select the about:blank tab again");
  gBrowser.selectedTab = tab;

  synthesizeToggleToolboxKey();

  info("Wait for the about:devtools tab to be selected");
  await waitUntil(() => isAboutDevtoolsTab(gBrowser.selectedTab));

  // filter is not available on gBrowser.tabs.
  const aboutDevtoolsTabs = [...gBrowser.tabs].filter(isAboutDevtoolsTab);
  ok(aboutDevtoolsTabs.length === 1, "Only one tab of about:devtools was opened.");
  ok(aboutDevtoolsTabs[0] === aboutDevtoolsTab,
    "The existing about:devtools tab was reused.");

  await removeTab(aboutDevtoolsTab);
  await removeTab(tab);
});
