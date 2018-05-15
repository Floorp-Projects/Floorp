/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that we setup a speculative network
// connection for autoFilled values.

let gHttpServer = null;
let gScheme = "http";
let gHost = "localhost"; // 'localhost' by default.
let gPort = -1;
let gPrivateWin = null;
let gIsSpeculativeConnected = false;

let gTest;

add_task(async function setup() {
  gHttpServer = runHttpServer(gScheme, gHost);
  // The server will be run on a random port if the port number wasn't given.
  gPort = gHttpServer.identity.primaryPort;

  gTest = {
    search: gHost.substr(0, 2),
    autofilledValue: `${gHost}:${gPort}/`
  };

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

  await PlacesTestUtils.addVisits([{
    uri: `${gScheme}://${gHost}:${gPort}`,
    title: "test visit for speculative connection",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  gPrivateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  is(PrivateBrowsingUtils.isWindowPrivate(gPrivateWin), true, "A private window created.");

  // Bug 764062 - we can't get port number from autocomplete result, so we have to mock
  // this function and add it manually.
  let oldSpeculativeConnect = gURLBar.popup.maybeSetupSpeculativeConnect.bind(gURLBar.popup);
  let newSpeculativeConnect = (uriString) => {
    gIsSpeculativeConnected = true;
    oldSpeculativeConnect(uriString);
  };
  gURLBar.popup.maybeSetupSpeculativeConnect = newSpeculativeConnect;
  gPrivateWin.gURLBar.popup.maybeSetupSpeculativeConnect = newSpeculativeConnect;

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    gURLBar.popup.maybeSetupSpeculativeConnect = oldSpeculativeConnect;
    gPrivateWin.gURLBar.popup.maybeSetupSpeculativeConnect = oldSpeculativeConnect;
    gHttpServer.identity.remove(gScheme, gHost, gPort);
    await new Promise(resolve => {
      gHttpServer.stop(() => {
        gHttpServer = null;
        resolve();
      });
    });
    await BrowserTestUtils.closeWindow(gPrivateWin);
    gScheme = null;
    gHost = null;
    gPort = null;
    gPrivateWin = null;
    gIsSpeculativeConnected = null;
    gTest = null;
  });
});

add_task(async function autofill_tests() {
  gIsSpeculativeConnected = false;
  info(`Searching for '${gTest.search}'`);
  await promiseAutocompleteResultPopup(gTest.search, window, true);
  is(gURLBar.inputField.value, gTest.autofilledValue,
     `Autofilled value is as expected for search '${gTest.search}'`);
  is(gIsSpeculativeConnected, true, "Speculative connection should be called");
  await promiseSpeculativeConnection(gHttpServer);
  is(gHttpServer.connectionNumber, 1, `${gHttpServer.connectionNumber} speculative connection has been setup.`);
});

add_task(async function privateContext_test() {
  info("In private context.");
  gIsSpeculativeConnected = false;
  info(`Searching for '${gTest.search}'`);
  await promiseAutocompleteResultPopup(gTest.search, gPrivateWin, true);
  is(gPrivateWin.gURLBar.inputField.value, gTest.autofilledValue,
     `Autofilled value is as expected for search '${gTest.search}'`);
  is(gIsSpeculativeConnected, false, "Speculative connection shouldn't be called");
});
