/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the This Firefox page is displayed by default when opening the new
 * about:debugging and that it contains the expected categories.
 */

const EXPECTED_TARGET_PANES = [
  "Temporary Extensions",
  "Extensions",
  "Tabs",
  "Service Workers",
  "Shared Workers",
  "Other Workers",
];

add_task(async function() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Check that the selected sidebar item is "This Firefox"/"This Nightly"/...
  const selectedSidebarItem = document.querySelector(".js-sidebar-item-selected");
  ok(selectedSidebarItem, "An item is selected in the sidebar");

  const thisFirefoxString = getThisFirefoxString(window);
  is(selectedSidebarItem.textContent, thisFirefoxString,
    "The selected sidebar item is " + thisFirefoxString);

  const paneTitlesEls = document.querySelectorAll(".js-debug-target-pane-title");
  is(paneTitlesEls.length, EXPECTED_TARGET_PANES.length,
    "This Firefox has the expecte number of debug target categories");

  const paneTitles = [...paneTitlesEls].map(el => el.textContent);

  for (let i = 0; i < EXPECTED_TARGET_PANES.length; i++) {
    const expectedPaneTitle = EXPECTED_TARGET_PANES[i];
    const actualPaneTitle = paneTitles[i];
    ok(actualPaneTitle.startsWith(expectedPaneTitle),
       `Expected debug target category found: ${ expectedPaneTitle }`);
  }

  await removeTab(tab);
});
