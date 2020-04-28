/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/*
 * Test loading inline scripts from cache:
 *   - Load document with inline script
 *   - Reload inside debugger with toolbox caching disabled
 *   - Reload inside debugger with toolbox caching enabled
 */

// Breakpoint position calculations can throw when interrupted by a navigation.
PromiseTestUtils.whitelistRejectionsGlobally(/Resource .*? does not exist/);

const server = createTestHTTPServer();

let docValue = 1;
server.registerPathHandler("/inline-cache.html", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  // HTTP should assume cacheable by default.
  // Toolbox defaults to disabling cache for subsequent loads when open.
  response.setHeader("Cache-Control", "public, max-age=3600");
  response.write(`
    <html>
    <head>
    <script>
      let x = ${docValue};
    </script>
    </head>
    </html>
  `);
});

const SOURCE_URL = `http://localhost:${
  server.identity.primaryPort
}/inline-cache.html`;

add_task(async function() {
  info("Load document with inline script");
  const tab = await addTab(SOURCE_URL);
  info("Open debugger");
  clearDebuggerPreferences();
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  info("Reload tab to ensure debugger finds script");
  await reloadTabAndDebugger(tab, dbg);
  let pageValue = await getPageValue(tab);
  is(pageValue, "let x = 1;", "Content loads from network, has doc value 1");
  await waitForSource(dbg, "inline-cache.html");
  await selectSource(dbg, "inline-cache.html");
  await waitForLoadedSource(dbg, "inline-cache.html");
  let dbgValue = findSourceContent(dbg, "inline-cache.html");
  info(`Debugger text: ${dbgValue.value}`);
  ok(
    dbgValue.value.includes(pageValue),
    "Debugger loads from cache, gets value 1 like page"
  );

  info("Disable HTTP cache for page");
  await toolbox.target.reconfigure({ options: { cacheDisabled: true } });
  makeChanges();

  info("Reload inside debugger with toolbox caching disabled (attempt 1)");
  await reloadTabAndDebugger(tab, dbg);
  pageValue = await getPageValue(tab);
  is(pageValue, "let x = 2;", "Content loads from network, has doc value 2");
  await waitForLoadedSource(dbg, "inline-cache.html");
  dbgValue = findSourceContent(dbg, "inline-cache.html");

  info(`Debugger text: ${dbgValue.value}`);
  ok(
    dbgValue.value.includes(pageValue),
    "Debugger loads from network, gets value 2 like page"
  );

  makeChanges();

  info("Reload inside debugger with toolbox caching disabled (attempt 2)");
  await reloadTabAndDebugger(tab, dbg);
  pageValue = await getPageValue(tab);
  is(pageValue, "let x = 3;", "Content loads from network, has doc value 3");
  await waitForLoadedSource(dbg, "inline-cache.html");
  dbgValue = findSourceContent(dbg, "inline-cache.html");
  info(`Debugger text: ${dbgValue.value}`);
  ok(
    dbgValue.value.includes(pageValue),
    "Debugger loads from network, gets value 3 like page"
  );

  info("Enable HTTP cache for page");
  await toolbox.target.reconfigure({ options: { cacheDisabled: false } });
  makeChanges();

  // Even though the HTTP cache is now enabled, Gecko sets the VALIDATE_ALWAYS flag when
  // reloading the page.  So, it will always make a request to the server for the main
  // document contents.

  info("Reload inside debugger with toolbox caching enabled (attempt 1)");
  await reloadTabAndDebugger(tab, dbg);
  pageValue = await getPageValue(tab);
  is(pageValue, "let x = 4;", "Content loads from network, has doc value 4");
  await waitForLoadedSource(dbg, "inline-cache.html");
  dbgValue = findSourceContent(dbg, "inline-cache.html");
  info(`Debugger text: ${dbgValue.value}`);
  ok(
    dbgValue.value.includes(pageValue),
    "Debugger loads from cache, gets value 4 like page"
  );

  makeChanges();

  info("Reload inside debugger with toolbox caching enabled (attempt 2)");
  await reloadTabAndDebugger(tab, dbg);
  pageValue = await getPageValue(tab);
  is(pageValue, "let x = 5;", "Content loads from network, has doc value 5");
  await waitForLoadedSource(dbg, "inline-cache.html");
  await waitForSelectedSource(dbg, "inline-cache.html");
  dbgValue = findSourceContent(dbg, "inline-cache.html");
  info(`Debugger text: ${dbgValue.value}`);
  ok(
    dbgValue.value.includes(pageValue),
    "Debugger loads from cache, gets value 5 like page"
  );

  await toolbox.destroy();
  await removeTab(tab);
});

/**
 * This is meant to simulate the developer editing the inline source and saving.
 * Effectively, we change the source during the test at specific controlled points.
 */
function makeChanges() {
  docValue++;
}

function getPageValue(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    return content.document.querySelector("script").textContent.trim();
  });
}

async function reloadTabAndDebugger(tab, dbg) {
  let navigated = waitForDispatch(dbg, "NAVIGATE");
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await reload(dbg, "inline-cache.html");
  return Promise.all([navigated, loaded]);
}
