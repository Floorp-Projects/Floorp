/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that we setup a speculative network connection to
// the site in mousedown event before the http request happens(in mouseup).

let gHttpServer = null;
let gScheme = "http";
let gHost = "localhost"; // 'localhost' by default.
let gPort = -1;
let gIsSpeculativeConnected = false;

add_task(async function setup() {
  gHttpServer = runHttpServer(gScheme, gHost, gPort);
  // The server will be run on a random port if the port number wasn't given.
  gPort = gHttpServer.identity.primaryPort;

  await PlacesTestUtils.addVisits([{
    uri: `${gScheme}://${gHost}:${gPort}`,
    title: "test visit for speculative connection",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", true],
          // Turn off search suggestion so we won't speculative connect to the search engine.
          ["browser.search.suggest.enabled", false],
          ["browser.urlbar.speculativeConnect.enabled", true],
          // In mochitest this number is 0 by default but we have to turn it on.
          ["network.http.speculative-parallel-limit", 6],
          // The http server is using IPv4, so it's better to disable IPv6 to avoid weird
          // networking problem.
          ["network.dns.disableIPv6", true]],
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    gHttpServer.identity.remove(gScheme, gHost, gPort);
    gHttpServer.stop(() => {
      gHttpServer = null;
    });
  });
});

add_task(async function popup_mousedown_tests() {
  const test = {
    // To not trigger autofill, search keyword starts from the second character.
    search: gHost.substr(1, 4),
    completeValue: `${gScheme}://${gHost}:${gPort}/`
  };
  info(`Searching for '${test.search}'`);
  await promiseAutocompleteResultPopup(test.search, window, true);
  // Check if the first result is with type "searchengine"
  let controller = gURLBar.popup.input.controller;
  // The first item should be 'Search with ...' thus we wan the second.
  let value = controller.getFinalCompleteValueAt(1);
  info(`The value of the second item is ${value}`);
  is(value, test.completeValue, "The second item has the url we visited.");

  await BrowserTestUtils.waitForCondition(() => {
    return !!gURLBar.popup.richlistbox.childNodes[1] &&
           BrowserTestUtils.is_visible(gURLBar.popup.richlistbox.childNodes[1]);
  }, "the node is there.");

  let listitem = gURLBar.popup.richlistbox.childNodes[1];
  EventUtils.synthesizeMouse(listitem, 10, 10, {type: "mousedown"}, window);
  is(gURLBar.popup.richlistbox.selectedIndex, 1, "The second item is selected");
  await promiseSpeculativeConnection(gHttpServer);
  is(gHttpServer.connectionNumber, 1, `${gHttpServer.connectionNumber} speculative connection has been setup.`);
});
