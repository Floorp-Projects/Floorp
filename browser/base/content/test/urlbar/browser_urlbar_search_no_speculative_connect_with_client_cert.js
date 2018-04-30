/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that we don't speculatively connect when user certificates are installed

const { MockRegistrar } =
  ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", {});

const certService = Cc["@mozilla.org/security/local-cert-service;1"]
                      .getService(Ci.nsILocalCertService);
const certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);

const host = "localhost";
let uri;
let handshakeDone = false;
let expectingChooseCertificate = false;
let chooseCertificateCalled = false;

const clientAuthDialogs = {
  chooseCertificate(ctx, hostname, port, organization, issuerOrg, certList,
                    selectedIndex) {
    ok(expectingChooseCertificate,
       `${expectingChooseCertificate ? "" : "not "}expecting chooseCertificate to be called`);
    is(certList.length, 1, "should have only one client certificate available");
    selectedIndex.value = 0;
    chooseCertificateCalled = true;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIClientAuthDialogs]),
};

function startServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"]
                    .createInstance(Ci.nsITLSServerSocket);
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;

  let input, output;

  let listener = {
    onSocketAccepted(socket, transport) {
      info("Accepted TLS client connection");
      let connectionInfo = transport.securityInfo
                           .QueryInterface(Ci.nsITLSServerConnectionInfo);
      connectionInfo.setSecurityObserver(listener);
      input = transport.openInputStream(0, 0, 0);
      output = transport.openOutputStream(0, 0, 0);
    },

    onHandshakeDone(socket, status) {
      info("TLS handshake done");
      handshakeDone = true;

      input.asyncWait({
        onInputStreamReady(readyInput) {
          try {
            let request = NetUtil.readInputStreamToString(readyInput,
                                                          readyInput.available());
            ok(request.startsWith("GET /") && request.includes("HTTP/1.1"),
               "expecting an HTTP/1.1 GET request");
            let response = "HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\n" +
                           "Connection:Close\r\nContent-Length:2\r\n\r\nOK";
            output.write(response, response.length);
          } catch (e) {
            // This will fail when we close the speculative connection.
          }
        }
      }, 0, 0, Services.tm.currentThread);
    },

    onStopListening() {
      info("onStopListening");
      input.close();
      output.close();
    }
  };

  tlsServer.setSessionCache(false);
  tlsServer.setSessionTickets(false);
  tlsServer.setRequestClientCertificate(Ci.nsITLSServerSocket.REQUEST_ALWAYS);

  tlsServer.asyncListen(listener);

  return tlsServer;
}

let server;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", true],
          // Turn off search suggestion so we won't speculative connect to the search engine.
          ["browser.search.suggest.enabled", false],
          ["browser.urlbar.speculativeConnect.enabled", true],
          // In mochitest this number is 0 by default but we have to turn it on.
          ["network.http.speculative-parallel-limit", 6],
          // The http server is using IPv4, so it's better to disable IPv6 to avoid weird
          // networking problem.
          ["network.dns.disableIPv6", true],
          ["security.default_personal_cert", "Ask Every Time"]],
  });

  let clientAuthDialogsCID =
    MockRegistrar.register("@mozilla.org/nsClientAuthDialogs;1",
                           clientAuthDialogs);

  let cert = await new Promise((resolve, reject) => {
    certService.getOrCreateCert("speculative-connect", {
      handleCert(c, rv) {
        if (!Components.isSuccessCode(rv)) {
          reject(rv);
          return;
        }
        resolve(c);
      }
    });
  });
  server = startServer(cert);
  uri = `https://${host}:${server.port}/`;
  info(`running tls server at ${uri}`);
  await PlacesTestUtils.addVisits([{
    uri,
    title: "test visit for speculative connection",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  let overrideBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                     Ci.nsICertOverrideService.ERROR_MISMATCH;
  certOverrideService.rememberValidityOverride("localhost", server.port, cert,
                                               overrideBits, true);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    MockRegistrar.unregister(clientAuthDialogsCID);
    certOverrideService.clearValidityOverride("localhost", server.port);
  });
});

add_task(async function popup_mousedown_no_client_cert_dialog_until_navigate_test() {
  const test = {
    // To not trigger autofill, search keyword starts from the second character.
    search: host.substr(1, 4),
    completeValue: uri
  };
  info(`Searching for '${test.search}'`);
  await promiseAutocompleteResultPopup(test.search, window, true);
  await waitForAutocompleteResultAt(1);
  let controller = gURLBar.popup.input.controller;
  // The first item should be 'Search with ...' thus we want the second.
  let value = controller.getFinalCompleteValueAt(1);
  info(`The value of the second item is ${value}`);
  is(value, test.completeValue, "The second item has the url we visited.");

  let listitem = await waitForAutocompleteResultAt(1);
  Assert.ok(is_visible(listitem), "The node is there.");

  expectingChooseCertificate = false;
  EventUtils.synthesizeMouseAtCenter(listitem, {type: "mousedown"}, window);
  is(gURLBar.popup.richlistbox.selectedIndex, 1, "The second item is selected");

  // We shouldn't have triggered a speculative connection, because a client
  // certificate is installed.
  SimpleTest.requestFlakyTimeout("Wait for UI");
  await new Promise(resolve => setTimeout(resolve, 200));

  // Now mouseup, expect that we choose a client certificate, and expect that
  // we successfully load a page.
  expectingChooseCertificate = true;
  EventUtils.synthesizeMouseAtCenter(listitem, {type: "mouseup"}, window);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  ok(chooseCertificateCalled, "chooseCertificate must have been called");
  server.close();
});
