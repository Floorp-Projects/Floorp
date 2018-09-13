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
  const { document, tab } = await openAboutDebugging();

  // Wait until the client connection was established.
  await waitUntil(() => document.querySelector(".js-runtime-page"));

  // Check that the selected sidebar item is "This Firefox"
  const selectedSidebarItem = document.querySelector(".js-sidebar-item-selected");
  ok(selectedSidebarItem, "An item is selected in the sidebar");
  is(selectedSidebarItem.textContent, "This Firefox",
    "The selected sidebar item is This Firefox");

  const paneTitlesEls = document.querySelectorAll(".js-debug-target-pane-title");
  is(paneTitlesEls.length, EXPECTED_TARGET_PANES.length,
    "This Firefox has the expecte number of debug target categories");

  const paneTitles = [...paneTitlesEls].map(el => el.textContent);

  EXPECTED_TARGET_PANES.forEach(expectedPane => {
    ok(paneTitles.includes(expectedPane),
      "Expected debug target category found: " + expectedPane);
  });

  await removeTab(tab);
});
