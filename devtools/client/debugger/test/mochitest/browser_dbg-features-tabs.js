/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focuses on the Tabs component, where we display all opened sources.
 */

"use strict";

const testServer = createVersionizedHttpTestServer(
  "examples/sourcemaps-reload-uncompressed"
);
const TEST_URL = testServer.urlFor("index.html");

add_task(async function() {
  // We open against a blank page and only then navigate to the test page
  // so that sources aren't GC-ed before opening the debugger.
  // When we (re)load a page while the debugger is opened, the debugger
  // will force all sources to be held in memory.
  const dbg = await initDebuggerWithAbsoluteURL("about:blank");

  await navigateToAbsoluteURL(dbg, TEST_URL, ...INTEGRATION_TEST_PAGE_SOURCES);

  info("Wait for all sources to be displayed in the source tree");
  let displayedSources;
  await waitFor(() => {
    displayedSources = dbg.selectors.getDisplayedSourcesList();
    return displayedSources.length == INTEGRATION_TEST_PAGE_SOURCES.length;
  }, "Got the expected number of sources from the selectors");

  // Open each visible source in tabs
  const uniqueUrls = new Set();
  for (const source of displayedSources) {
    info(`Opening '${source.url}' from ${source.thread}`);
    await selectSource(dbg, source);
    uniqueUrls.add(source.url);
  }

  // Some sources are loaded from the same URL and only one tab will be opened for them
  is(countTabs(dbg), uniqueUrls.size, "Got a tab for each distinct source URL");

  for (const source of displayedSources) {
    info(`Closing '${source.url}' from ${source.thread}`);
    await closeTab(dbg, source);
  }

  is(countTabs(dbg), 0, "All tabs are closed");
});
