/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that we setup a speculative network connection to
// current search engine if the first result is 'searchengine'.

let gHttpServer = null;
let gScheme = "http";
let gHost = "localhost"; // 'localhost' by default.
let gPort = 20709; // the port number must be identical to what we said in searchSuggestionEngine2.xml
const TEST_ENGINE_BASENAME = "searchSuggestionEngine2.xml";

add_task(async function setup() {
  gHttpServer = runHttpServer(gScheme, gHost, gPort);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", true],
          // Make sure search suggestion for location bar is enabled
          ["browser.search.suggest.enabled", true],
          ["browser.urlbar.suggest.searches", true],
          ["browser.urlbar.speculativeConnect.enabled", true],
          // In mochitest this number is 0 by default but we have to turn it on.
          ["network.http.speculative-parallel-limit", 6],
          // The http server is using IPv4, so it's better to disable IPv6 to avoid weird
          // networking problem.
          ["network.dns.disableIPv6", true]],
  });

  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    Services.search.currentEngine = oldCurrentEngine;
    gHttpServer.identity.remove(gScheme, gHost, gPort);
    gHttpServer.stop(() => {
      gHttpServer = null;
    });
  });
});

add_task(async function connect_search_engine_tests() {
  info("Searching for 'foo'");
  await promiseAutocompleteResultPopup("foo", window, true);
  // Check if the first result is with type "searchengine"
  let controller = gURLBar.popup.input.controller;
  let style = controller.getStyleAt(0);
  is(style.includes("searchengine"), true, "The first result type is searchengine");
  await promiseSpeculativeConnection(gHttpServer);
  is(gHttpServer.connectionNumber, 1, `${gHttpServer.connectionNumber} speculative connection has been setup.`);
});
