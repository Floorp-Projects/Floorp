/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests filters.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-filters.html";

add_task(async function () {
  await pushPref("dom.security.https_first", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  const filterState = await getFilterState(hud);

  // Triggers network requests
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const url = "./sjs_slow-response-test-server.sjs";
    // Set a smaller delay for the 300 to ensure we get it before the "error" responses.
    content.fetch(`${url}?status=300&delay=100`);
    content.fetch(`${url}?status=404&delay=500`);
    content.fetch(`${url}?status=500&delay=500`);
  });

  // Wait for the messages
  await waitFor(() => findErrorMessage(hud, "status=404", ".network"));
  await waitFor(() => findErrorMessage(hud, "status=500", ".network"));

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
  is(
    findAllMessages(hud).length,
    7,
    "Messages of all levels shown when filters are on."
  );

  // Check that messages are not shown when their filter is turned off.
  await setFilterState(hud, {
    error: false,
  });
  await waitFor(() => findAllMessages(hud).length == 4);
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
  ok(
    !filterIsEnabled(filterBar.querySelector("[data-category='error']")),
    "Filter button setting is persisted"
  );
  is(
    findAllMessages(hud).length,
    4,
    "testFilterPersistence: Messages of all levels but error shown."
  );

  await resetFilters(hud);
}
