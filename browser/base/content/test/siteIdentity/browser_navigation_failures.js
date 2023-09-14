/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the site identity indicator is properly updated for navigations
// that fail for various reasons. In particular, we currently test TLS handshake
// failures, about: pages that don't actually exist, and situations where the
// TLS handshake completes but the server then closes the connection.
// See bug 1492424, bug 1493427, and bug 1391207.

const kSecureURI =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";
add_task(async function () {
  await BrowserTestUtils.withNewTab(kSecureURI, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    const TLS_HANDSHAKE_FAILURE_URI = "https://ssl3.example.com/";
    // Try to connect to a server where the TLS handshake will fail.
    BrowserTestUtils.startLoadingURIString(browser, TLS_HANDSHAKE_FAILURE_URI);
    await BrowserTestUtils.browserLoaded(
      browser,
      false,
      TLS_HANDSHAKE_FAILURE_URI,
      true
    );

    let newIdentityMode =
      window.document.getElementById("identity-box").className;
    is(
      newIdentityMode,
      "certErrorPage notSecureText",
      "identity should be unknown (not secure) after"
    );
  });
});

add_task(async function () {
  await BrowserTestUtils.withNewTab(kSecureURI, async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "verifiedDomain", "identity should be secure before");

    const BAD_ABOUT_PAGE_URI = "about:somethingthatdoesnotexist";
    // Try to load an about: page that doesn't exist
    BrowserTestUtils.startLoadingURIString(browser, BAD_ABOUT_PAGE_URI);
    await BrowserTestUtils.browserLoaded(
      browser,
      false,
      BAD_ABOUT_PAGE_URI,
      true
    );

    let newIdentityMode =
      window.document.getElementById("identity-box").className;
    is(
      newIdentityMode,
      "unknownIdentity",
      "identity should be unknown (not secure) after"
    );
  });
});

// Helper function to start a TLS server that will accept a connection, complete
// the TLS handshake, but then close the connection.
function startServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"].createInstance(
    Ci.nsITLSServerSocket
  );
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;

  let input, output;

  let listener = {
    onSocketAccepted(socket, transport) {
      let connectionInfo = transport.securityCallbacks.getInterface(
        Ci.nsITLSServerConnectionInfo
      );
      connectionInfo.setSecurityObserver(listener);
      input = transport.openInputStream(0, 0, 0);
      output = transport.openOutputStream(0, 0, 0);
    },

    onHandshakeDone(socket, status) {
      input.asyncWait(
        {
          onInputStreamReady(readyInput) {
            try {
              input.close();
              output.close();
            } catch (e) {
              info(e);
            }
          },
        },
        0,
        0,
        Services.tm.currentThread
      );
    },

    onStopListening() {},
  };

  tlsServer.setSessionTickets(false);
  tlsServer.asyncListen(listener);

  return tlsServer;
}

// Test that if we complete a TLS handshake but the server closes the connection
// just after doing so (resulting in a "connection reset" error page), the site
// identity information gets updated appropriately (it should indicate "not
// secure").
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    // This test fails on some platforms if we leave IPv6 enabled.
    set: [["network.dns.disableIPv6", true]],
  });

  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);

  let cert = getTestServerCertificate();
  // Start a server and trust its certificate.
  let server = startServer(cert);
  certOverrideService.rememberValidityOverride(
    "localhost",
    server.port,
    {},
    cert,
    true
  );

  // Un-do configuration changes we've made when the test is done.
  registerCleanupFunction(() => {
    certOverrideService.clearValidityOverride("localhost", server.port, {});
    server.close();
  });

  // Open up a new tab...
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const TLS_HANDSHAKE_FAILURE_URI = `https://localhost:${server.port}/`;
    // Try to connect to a server where the TLS handshake will succeed, but then
    // the server closes the connection right after.
    BrowserTestUtils.startLoadingURIString(browser, TLS_HANDSHAKE_FAILURE_URI);
    await BrowserTestUtils.browserLoaded(
      browser,
      false,
      TLS_HANDSHAKE_FAILURE_URI,
      true
    );

    let identityMode = window.document.getElementById("identity-box").className;
    is(
      identityMode,
      "certErrorPage notSecureText",
      "identity should be 'unknown'"
    );
  });
});
