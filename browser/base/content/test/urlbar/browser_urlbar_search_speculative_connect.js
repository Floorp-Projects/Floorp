"use strict";

// This test ensures that we setup a speculative network
// connection for autoFilled values.

let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});
let gHttpServer = null;
let gScheme = "http";
let gHost = "localhost"; // 'localhost' by default.
let gPort = -1;

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
          ["browser.urlbar.speculativeConnection.enabled", true],
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

  // Bug 764062 - we can't get port number from autocomplete result, so we have to mock
  // this function to add it manually.
  let oldSpeculativeConnect = gURLBar.popup.maybeSetupSpeculativeConnect.bind(gURLBar.popup);
  gURLBar.popup.maybeSetupSpeculativeConnect = (uriString) => {
    info(`Original uri is ${uriString}`);
    let newUriString = uriString.substr(0, uriString.length - 1) +
                       ":" + gPort + "/";
    info(`New uri is ${newUriString}`);
    oldSpeculativeConnect(newUriString);
  };

  registerCleanupFunction(async function() {
    await PlacesTestUtils.clearHistory();
    gURLBar.popup.maybeSetupSpeculativeConnect = oldSpeculativeConnect;
    gHttpServer.identity.remove(gScheme, gHost, gPort);
    gHttpServer.stop(() => {
      gHttpServer = null;
    });
  });
});

add_task(async function autofill_tests() {
  const test = {
    search: gHost.substr(0, 2),
    autofilledValue: `${gHost}/`
  };

  info(`Searching for '${test.search}'`);
  await promiseAutocompleteResultPopup(test.search, window, true);
  is(gURLBar.inputField.value, test.autofilledValue,
     `Autofilled value is as expected for search '${test.search}'`);

  await BrowserTestUtils.waitForCondition(() => {
    if (gHttpServer) {
      is(gHttpServer.connectionNumber, 1,
         `${gHttpServer.connectionNumber} speculative connection has been setup.`)
      return gHttpServer.connectionNumber == 1;
    }
    return false;
  }, "Waiting for connection setup");
});

