/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that, if browser.chrome.guess_favicon is enabled, the favicon.ico will
// be loaded for the JSON viewer. The favicon could be prevented from loading
// by a too strict CSP (Bug 1872504).

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

add_task(async function test_favicon() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.chrome.guess_favicon", true]],
  });

  const httpServer = new HttpServer();

  httpServer.registerPathHandler("/", (_, response) => {
    response.setHeader("Content-Type", "application/json");
    response.write("{}");
  });

  const faviconPromise = new Promise(resolve => {
    httpServer.registerPathHandler("/favicon.ico", () => {
      resolve();
    });
  });

  httpServer.start(-1);

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: `http://localhost:${httpServer.identity.primaryPort}/`,
  });

  info("Waiting for favicon request to be received by server");
  await faviconPromise;
  ok("Server got request for favicon");

  BrowserTestUtils.removeTab(tab);

  httpServer.stop();
});
