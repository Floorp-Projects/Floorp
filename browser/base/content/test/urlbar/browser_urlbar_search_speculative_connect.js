/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that we setup a speculative network
// connection for autoFilled values.

let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});
let gHttpServer = null;
let gScheme = "http";
let gHost = "localhost"; // 'localhost' by default.
let gPort = -1;
let gPrivateWin = null;
let gIsSpeculativeConnected = false;

add_task(async function setup() {
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    try {
      gHttpServer.start(gPort);
      gPort = gHttpServer.identity.primaryPort;
      gHttpServer.identity.setPrimary(gScheme, gHost, gPort);
    } catch (ex) {
      info("We can't launch our http server successfully.")
    }
  }
  is(gHttpServer.identity.has(gScheme, gHost, gPort), true, "make sure we have this domain listed");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", true],
          // Turn off speculative connect to the search engine.
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
  // this function to add it manually.
  let oldSpeculativeConnect = gURLBar.popup.maybeSetupSpeculativeConnect.bind(gURLBar.popup);
  let newSpeculativeConnect = (uriString) => {
    gIsSpeculativeConnected = true;
    info(`Original uri is ${uriString}`);
    let newUriString = uriString.substr(0, uriString.length - 1) +
                       ":" + gPort + "/";
    info(`New uri is ${newUriString}`);
    oldSpeculativeConnect(newUriString);
  };
  gURLBar.popup.maybeSetupSpeculativeConnect = newSpeculativeConnect;
  gPrivateWin.gURLBar.popup.maybeSetupSpeculativeConnect = newSpeculativeConnect;

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    gURLBar.popup.maybeSetupSpeculativeConnect = oldSpeculativeConnect;
    gPrivateWin.gURLBar.popup.maybeSetupSpeculativeConnect = oldSpeculativeConnect;
    gHttpServer.identity.remove(gScheme, gHost, gPort);
    gHttpServer.stop(() => {
      gHttpServer = null;
    });
    await BrowserTestUtils.closeWindow(gPrivateWin);
  });
});

const test = {
  search: gHost.substr(0, 2),
  autofilledValue: `${gHost}/`
};

add_task(async function autofill_tests() {
  gIsSpeculativeConnected = false;
  info(`Searching for '${test.search}'`);
  await promiseAutocompleteResultPopup(test.search, window, true);
  is(gURLBar.inputField.value, test.autofilledValue,
     `Autofilled value is as expected for search '${test.search}'`);
  is(gIsSpeculativeConnected, true, "Speculative connection should be called");
  await promiseSpeculativeConnection(gHttpServer);
});

add_task(async function privateContext_test() {
  info("In private context.");
  gIsSpeculativeConnected = false;
  info(`Searching for '${test.search}'`);
  await promiseAutocompleteResultPopup(test.search, gPrivateWin, true);
  is(gPrivateWin.gURLBar.inputField.value, test.autofilledValue,
     `Autofilled value is as expected for search '${test.search}'`);
  is(gIsSpeculativeConnected, false, "Speculative connection shouldn't be called");
});
