/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that if we're attempting to speculatively connect to a site but it
// requests a client certificate, we cancel the speculative connection (this
// avoids an unexpected "select a client certificate" dialog).

const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});

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

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIClientAuthDialogs]),
};

function startServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"]
                    .createInstance(Ci.nsITLSServerSocket);
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;

  let input, output;

  let listener = {
    onSocketAccepted: function(socket, transport) {
      info("Accepted TLS client connection");
      let connectionInfo = transport.securityInfo
                           .QueryInterface(Ci.nsITLSServerConnectionInfo);
      connectionInfo.setSecurityObserver(listener);
      input = transport.openInputStream(0, 0, 0);
      output = transport.openOutputStream(0, 0, 0);
    },

    onHandshakeDone: function(socket, status) {
      info("TLS handshake done");
      handshakeDone = true;

      input.asyncWait({
        onInputStreamReady: function(input) {
          try {
            let request = NetUtil.readInputStreamToString(input,
                                                          input.available());
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

    onStopListening: function() {
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
      handleCert: function(c, rv) {
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
  let controller = gURLBar.popup.input.controller;
  // The first item should be 'Search with ...' thus we want the second.
  let value = controller.getFinalCompleteValueAt(1);
  info(`The value of the second item is ${value}`);
  is(value, test.completeValue, "The second item has the url we visited.");

  await BrowserTestUtils.waitForCondition(() => {
    return !!gURLBar.popup.richlistbox.childNodes[1] &&
           is_visible(gURLBar.popup.richlistbox.childNodes[1]);
  }, "the node is there.");

  let listitem = gURLBar.popup.richlistbox.childNodes[1];
  EventUtils.synthesizeMouse(listitem, 10, 10, {type: "mousedown"}, window);
  is(gURLBar.popup.richlistbox.selectedIndex, 1, "The second item is selected");
  // Since we don't know before connecting that a server will request a client
  // certificate, we actually do make a speculative connection. The trick is
  // that we cancel it and don't re-use it if we are asked for a certificate
  // (and of course we don't show the certificate-picking UI). So, the TLS
  // server will actually complete the handshake.
  await BrowserTestUtils.waitForCondition(() => handshakeDone,
                                          "waiting for handshake to complete");
  // Now mouseup, expect that we choose a client certificate, and expect that we
  // successfully load a page.
  expectingChooseCertificate = true;
  EventUtils.synthesizeMouse(listitem, 10, 10, {type: "mouseup"}, window);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  ok(chooseCertificateCalled, "chooseCertificate must have been called");
  server.close();
});
