/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the Browser Console does not use the same filter prefs as the Web
// Console.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>browser console filters";

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);
  ok(hud, "web console opened");

  let filterState = await getFilterState(hud);
  ok(filterState.error, "The web console error filter is enabled");

  info(`toggle "error" filter`);
  await setFilterState(hud, {
    error: false,
  });

  filterState = await getFilterState(hud);
  ok(!filterState.error, "The web console error filter is disabled");

  await resetFilters(hud);
  await closeConsole();
  info("web console closed");

  hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console opened");

  filterState = await getFilterState(hud);
  ok(filterState.error, "The browser console error filter is enabled");

  info(`toggle "error" filter in browser console`);
  await setFilterState(hud, {
    error: false,
  });

  filterState = await getFilterState(hud);
  ok(!filterState.error, "The browser console error filter is disabled");

  await resetFilters(hud);
});
