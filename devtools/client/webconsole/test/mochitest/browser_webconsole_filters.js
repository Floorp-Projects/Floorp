/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests filters.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console-filters.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await setFilterBarVisible(hud, true);

  const filterState = await getFilterState(hud);

  // Check defaults.

  for (const category of ["error", "warn", "log", "info", "debug"]) {
    const state = filterState[category];
    ok(state, `Filter button for ${category} is on by default`);
  }
  for (const category of ["css", "net", "netxhr"]) {
    const state = filterState[category];
    ok(!state, `Filter button for ${category} is off by default`);
  }

  // Check that messages are shown as expected. This depends on cached messages being
  // shown.
  ok(findMessages(hud, "").length == 5,
    "Messages of all levels shown when filters are on.");

  // Check that messages are not shown when their filter is turned off.
  await setFilterState(hud, {
    error: false
  });
  await waitFor(() => findMessages(hud, "").length == 4);
  ok(true, "When a filter is turned off, its messages are not shown.");

  // Check that the ui settings were persisted.
  await closeTabAndToolbox();
  await testFilterPersistence();
});

function filterIsEnabled(button) {
  return button.classList.contains("checked");
}

async function testFilterPersistence() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const outputNode = hud.ui.outputNode;
  const filterBar = await waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-secondary");
  });
  ok(filterBar, "Filter bar ui setting is persisted.");
  // Check that the filter settings were persisted.
  ok(!filterIsEnabled(filterBar.querySelector(".error")),
    "Filter button setting is persisted");
  ok(findMessages(hud, "").length == 4,
    "testFilterPersistence: Messages of all levels shown when filters are on.");

  await resetFilters(hud);
  await setFilterBarVisible(hud, false);
}
