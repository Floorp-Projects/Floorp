/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests all filters persist.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console-filters.html";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  let filterButtons = yield getFilterButtons(hud);
  info("Disable all filters");
  filterButtons.forEach(filterButton => {
    if (filterIsEnabled(filterButton)) {
      filterButton.click();
    }
  });

  info("Close and re-open the console");
  yield closeTabAndToolbox();
  hud = yield openNewTabAndConsole(TEST_URI);

  info("Check that all filters are disabled, and enable them");
  filterButtons = yield getFilterButtons(hud);
  filterButtons.forEach(filterButton => {
    ok(!filterIsEnabled(filterButton), "filter is disabled");
    filterButton.click();
  });

  info("Close and re-open the console");
  yield closeTabAndToolbox();
  hud = yield openNewTabAndConsole(TEST_URI);

  info("Check that all filters are enabled");
  filterButtons = yield getFilterButtons(hud);
  filterButtons.forEach(filterButton => {
    ok(filterIsEnabled(filterButton), "filter is enabled");
  });

  // Check that the ui settings were persisted.
  yield closeTabAndToolbox();
});

function* getFilterButtons(hud) {
  const outputNode = hud.ui.outputNode;

  info("Wait for console toolbar to appear");
  const toolbar = yield waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-primary");
  });

  // Show the filter bar if it is hidden
  if (!outputNode.querySelector(".webconsole-filterbar-secondary")) {
    toolbar.querySelector(".devtools-filter-icon").click();
  }

  info("Wait for console filterbar to appear");
  const filterBar = yield waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-secondary");
  });
  ok(filterBar, "Filter bar is shown when filter icon is clicked.");

  return filterBar.querySelectorAll(".devtools-button");
}

function filterIsEnabled(button) {
  return button.classList.contains("checked");
}
