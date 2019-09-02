/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests all filters persist.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-filters.html";

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);

  let filterButtons = await getFilterButtons(hud);
  info("Disable all filters");
  filterButtons.forEach(filterButton => {
    if (filterIsEnabled(filterButton)) {
      filterButton.click();
    }
  });

  info("Close and re-open the console");
  await closeTabAndToolbox();
  hud = await openNewTabAndConsole(TEST_URI);

  info("Check that all filters are disabled, and enable them");
  filterButtons = await getFilterButtons(hud);
  filterButtons.forEach(filterButton => {
    ok(!filterIsEnabled(filterButton), "filter is disabled");
    filterButton.click();
  });

  // Wait for the CSS warning to be displayed so we don't have a pending promise.
  await waitFor(() => findMessage(hud, "Expected color but found ‘blouge’"));

  info("Close and re-open the console");
  await closeTabAndToolbox();
  hud = await openNewTabAndConsole(TEST_URI);

  info("Check that all filters are enabled");
  filterButtons = await getFilterButtons(hud);
  filterButtons.forEach(filterButton => {
    ok(filterIsEnabled(filterButton), "filter is enabled");
  });
  // Check that the ui settings were persisted.
  await closeTabAndToolbox();
});

async function getFilterButtons(hud) {
  const outputNode = hud.ui.outputNode;

  info("Wait for console filterbar to appear");
  const filterBar = await waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-secondary");
  });
  ok(filterBar, "Filter bar is shown when filter icon is clicked.");

  return filterBar.querySelectorAll(".devtools-togglebutton");
}

function filterIsEnabled(button) {
  return button.getAttribute("aria-pressed") === "true";
}
