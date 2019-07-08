/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that we setup a speculative network connection to
// the site in various cases:
//  1. search engine if it's the first result
//  2. mousedown event before the http request happens(in mouseup).

const TEST_ENGINE_BASENAME = "searchSuggestionEngine2.xml";

const serverInfo = {
  scheme: "http",
  host: "localhost",
  port: 20709, // Must be identical to what is in searchSuggestionEngine2.xml
};

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.speculativeConnect.enabled", true],
      // In mochitest this number is 0 by default but we have to turn it on.
      ["network.http.speculative-parallel-limit", 6],
      // The http server is using IPv4, so it's better to disable IPv6 to avoid
      // weird networking problem.
      ["network.dns.disableIPv6", true],
    ],
  });

  // Ensure we start from a clean situation.
  await PlacesUtils.history.clear();

  await PlacesTestUtils.addVisits([
    {
      uri: `${serverInfo.scheme}://${serverInfo.host}:${serverInfo.port}`,
      title: "test visit for speculative connection",
      transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
    },
  ]);

  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  let oldCurrentEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    Services.search.defaultEngine = oldCurrentEngine;
  });
});

add_task(async function search_test() {
  // We speculative connect to the search engine only if suggestions are enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.suggest.enabled", true]],
  });
  await withHttpServer(serverInfo, async server => {
    let connectionNumber = server.connectionNumber;
    info("Searching for 'foo'");
    await promiseAutocompleteResultPopup("foo", window, true);
    // Check if the first result is with type "searchengine"
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(
      details.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "The first result is a search"
    );
    await UrlbarTestUtils.promiseSpeculativeConnections(
      server,
      connectionNumber + 1
    );
  });
});

add_task(async function popup_mousedown_test() {
  // Disable search suggestions and autofill, to avoid other speculative
  // connections.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", false],
      ["browser.urlbar.autoFill", false],
    ],
  });
  await withHttpServer(serverInfo, async server => {
    let connectionNumber = server.connectionNumber;
    let searchString = "ocal";
    let completeValue = `${serverInfo.scheme}://${serverInfo.host}:${
      serverInfo.port
    }/`;
    info(`Searching for '${searchString}'`);

    await promiseAutocompleteResultPopup(searchString, window, true);
    let listitem = await waitForAutocompleteResultAt(1);
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    Assert.equal(
      details.url,
      completeValue,
      "The second item has the url we visited."
    );

    info("Clicking on the second result");
    EventUtils.synthesizeMouseAtCenter(listitem, { type: "mousedown" }, window);
    Assert.equal(
      UrlbarTestUtils.getSelectedElement(window),
      listitem,
      "The second item is selected"
    );
    await UrlbarTestUtils.promiseSpeculativeConnections(
      server,
      connectionNumber + 1
    );
  });
});

add_task(async function test_autofill() {
  // Disable search suggestions but enable autofill.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", false],
      ["browser.urlbar.autoFill", true],
    ],
  });
  await withHttpServer(serverInfo, async server => {
    let connectionNumber = server.connectionNumber;
    let searchString = serverInfo.host;
    info(`Searching for '${searchString}'`);

    await promiseAutocompleteResultPopup(searchString, window, true);
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    let completeValue = `${serverInfo.scheme}://${serverInfo.host}:${
      serverInfo.port
    }/`;
    Assert.equal(details.url, completeValue, `Autofilled value is as expected`);
    await UrlbarTestUtils.promiseSpeculativeConnections(
      server,
      connectionNumber + 1
    );
  });
});

add_task(async function test_autofill_privateContext() {
  info("Autofill in private context.");
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  registerCleanupFunction(async () => {
    let promisePBExit = TestUtils.topicObserved("last-pb-context-exited");
    await BrowserTestUtils.closeWindow(privateWin);
    await promisePBExit;
  });
  await withHttpServer(serverInfo, async server => {
    let connectionNumber = server.connectionNumber;
    let searchString = serverInfo.host;
    info(`Searching for '${searchString}'`);

    await promiseAutocompleteResultPopup(searchString, privateWin, true);
    let details = await UrlbarTestUtils.getDetailsOfResultAt(privateWin, 0);
    let completeValue = `${serverInfo.scheme}://${serverInfo.host}:${
      serverInfo.port
    }/`;
    Assert.equal(details.url, completeValue, `Autofilled value is as expected`);
    await UrlbarTestUtils.promiseSpeculativeConnections(
      server,
      connectionNumber
    );
  });
});

add_task(async function test_no_heuristic_result() {
  info("Don't speculative connect on results addition if there's no heuristic");
  await withHttpServer(serverInfo, async server => {
    let connectionNumber = server.connectionNumber;
    info(`Searching for the empty string`);
    await promiseAutocompleteResultPopup("", window, true);
    ok(UrlbarTestUtils.getResultCount(window) > 0, "Has results");
    let result = await UrlbarTestUtils.getSelectedElement(window);
    Assert.strictEqual(result, null, `Should have no selection`);
    await UrlbarTestUtils.promiseSpeculativeConnections(
      server,
      connectionNumber
    );
  });
});
