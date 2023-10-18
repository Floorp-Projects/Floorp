/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an insecure resource routed over a secure transport is considered
// insecure in terms of the site identity panel. We achieve this by running an
// HTTP-over-TLS "proxy" and having Firefox request an http:// URI over it.

const NOT_SECURE_LABEL = Services.prefs.getBoolPref(
  "security.insecure_connection_text.enabled"
)
  ? "notSecure notSecureText"
  : "notSecure";

/**
 * Tests that the page info dialog "security" section labels a
 * connection as unencrypted and does not show certificate.
 * @param {string} uri - URI of the page to test with.
 */
async function testPageInfoNotEncrypted(uri) {
  let pageInfo = BrowserPageInfo(uri, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(securityTab),
    "Security tab should be visible."
  );

  let secLabel = pageInfoDoc.getElementById("security-technical-shortform");
  await TestUtils.waitForCondition(
    () => secLabel.value == "Connection Not Encrypted",
    "pageInfo 'Security Details' should show not encrypted"
  );

  let viewCertBtn = pageInfoDoc.getElementById("security-view-cert");
  ok(
    viewCertBtn.collapsed,
    "pageInfo 'View Cert' button should not be visible"
  );
  pageInfo.close();
}

// But first, a quick test that we don't incorrectly treat a
// blob:https://example.com URI as secure.
add_task(async function () {
  let uri =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "dummy_page.html";
  await BrowserTestUtils.withNewTab(uri, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let debug = { hello: "world" };
      let blob = new Blob([JSON.stringify(debug, null, 2)], {
        type: "application/json",
      });
      let blobUri = URL.createObjectURL(blob);
      content.document.location = blobUri;
    });
    await BrowserTestUtils.browserLoaded(browser);
    let identityMode = window.document.getElementById("identity-box").className;
    is(identityMode, "localResource", "identity should be 'localResource'");
    await testPageInfoNotEncrypted(uri);
  });
});

// This server pretends to be a HTTP over TLS proxy. It isn't really, but this
// is sufficient for the purposes of this test.
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
              let request = NetUtil.readInputStreamToString(
                readyInput,
                readyInput.available()
              );
              ok(
                request.startsWith("GET ") && request.includes("HTTP/1.1"),
                "expecting an HTTP/1.1 GET request"
              );
              let response =
                "HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\n" +
                "Connection:Close\r\nContent-Length:2\r\n\r\nOK";
              output.write(response, response.length);
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

    onStopListening() {
      input.close();
      output.close();
    },
  };

  tlsServer.setSessionTickets(false);
  tlsServer.asyncListen(listener);

  return tlsServer;
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    // This test fails on some platforms if we leave IPv6 enabled.
    set: [["network.dns.disableIPv6", true]],
  });

  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);

  let cert = getTestServerCertificate();
  // Start the proxy and configure Firefox to trust its certificate.
  let server = startServer(cert);
  certOverrideService.rememberValidityOverride(
    "localhost",
    server.port,
    {},
    cert,
    true
  );
  // Configure Firefox to use the proxy.
  let systemProxySettings = {
    QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),
    mainThreadOnly: true,
    PACURI: null,
    getProxyForURI: (aSpec, aScheme, aHost, aPort) => {
      return `HTTPS localhost:${server.port}`;
    },
  };
  let oldProxyType = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref(
    "network.proxy.type",
    Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM
  );
  let { MockRegistrar } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistrar.sys.mjs"
  );
  let mockProxy = MockRegistrar.register(
    "@mozilla.org/system-proxy-settings;1",
    systemProxySettings
  );
  // Register cleanup to undo the configuration changes we've made.
  registerCleanupFunction(() => {
    certOverrideService.clearValidityOverride("localhost", server.port, {});
    Services.prefs.setIntPref("network.proxy.type", oldProxyType);
    MockRegistrar.unregister(mockProxy);
    server.close();
  });

  // Navigate to 'http://example.com'. Our proxy settings will route this via
  // the "proxy" we just started. Even though our connection to the proxy is
  // secure, in a real situation the connection from the proxy to
  // http://example.com won't be secure, so we treat it as not secure.
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com/", async browser => {
    let identityMode = window.document.getElementById("identity-box").className;
    is(
      identityMode,
      NOT_SECURE_LABEL,
      `identity should be '${NOT_SECURE_LABEL}'`
    );

    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    await testPageInfoNotEncrypted("http://example.com");
  });
});
