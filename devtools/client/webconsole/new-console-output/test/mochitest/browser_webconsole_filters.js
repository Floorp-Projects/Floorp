/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests filters.

"use strict";

const { MESSAGE_LEVEL } = require("devtools/client/webconsole/new-console-output/constants");

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console-filters.html";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  const outputNode = hud.ui.outputNode;

  const toolbar = yield waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-primary");
  });
  ok(toolbar, "Toolbar found");

  // Show the filter bar
  toolbar.querySelector(".devtools-filter-icon").click();
  const filterBar = yield waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-secondary");
  });
  ok(filterBar, "Filter bar is shown when filter icon is clicked.");

  // Check defaults.
  Object.values(MESSAGE_LEVEL).forEach(level => {
    ok(filterIsEnabled(filterBar.querySelector(`.${level}`)),
      `Filter button for ${level} is on by default`);
  });
  ["net", "netxhr"].forEach(category => {
    ok(!filterIsEnabled(filterBar.querySelector(`.${category}`)),
      `Filter button for ${category} is off by default`);
  });

  // Check that messages are shown as expected. This depends on cached messages being
  // shown.
  ok(findMessages(hud, "").length == 5,
    "Messages of all levels shown when filters are on.");

  // Check that messages are not shown when their filter is turned off.
  filterBar.querySelector(".error").click();
  yield waitFor(() => findMessages(hud, "").length == 4);
  ok(true, "When a filter is turned off, its messages are not shown.");

  // Check that the ui settings were persisted.
  yield closeTabAndToolbox();
  yield testFilterPersistence();
});

function filterIsEnabled(button) {
  return button.classList.contains("checked");
}

function* testFilterPersistence() {
  let hud = yield openNewTabAndConsole(TEST_URI);
  const outputNode = hud.ui.outputNode;
  const filterBar = yield waitFor(() => {
    return outputNode.querySelector(".webconsole-filterbar-secondary");
  });
  ok(filterBar, "Filter bar ui setting is persisted.");

  // Check that the filter settings were persisted.
  ok(!filterIsEnabled(filterBar.querySelector(".error")),
    "Filter button setting is persisted");
  ok(findMessages(hud, "").length == 4,
    "Messages of all levels shown when filters are on.");
}
