/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;
const MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT = MOZILLA_PKIX_ERROR_BASE + 14;

const IFRAME_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "dummy_iframe_page.html";

// Tests the getSecurityInfo() function exposed on WindowGlobalParent.
add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let loaded = BrowserTestUtils.waitForErrorPage(browser);
    await BrowserTestUtils.loadURI(browser, "https://self-signed.example.com");
    await loaded;

    let securityInfo = await browser.browsingContext.currentWindowGlobal.getSecurityInfo();
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    ok(securityInfo, "Found some security info");
    ok(securityInfo.failedCertChain, "Has a failed cert chain");
    is(
      securityInfo.errorCode,
      MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
      "Has the correct error code"
    );
    is(
      securityInfo.serverCert.commonName,
      "self-signed.example.com",
      "Has the correct certificate"
    );

    loaded = BrowserTestUtils.browserLoaded(browser);
    await BrowserTestUtils.loadURI(browser, "http://example.com");
    await loaded;

    securityInfo = await browser.browsingContext.currentWindowGlobal.getSecurityInfo();
    is(securityInfo, null, "Found no security info");

    loaded = BrowserTestUtils.browserLoaded(browser);
    await BrowserTestUtils.loadURI(browser, "https://example.com");
    await loaded;

    securityInfo = await browser.browsingContext.currentWindowGlobal.getSecurityInfo();
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    ok(securityInfo, "Found some security info");
    ok(securityInfo.succeededCertChain, "Has a succeeded cert chain");
    is(securityInfo.errorCode, 0, "Has no error code");
    is(
      securityInfo.serverCert.commonName,
      "example.com",
      "Has the correct certificate"
    );

    loaded = BrowserTestUtils.browserLoaded(browser);
    await BrowserTestUtils.loadURI(browser, IFRAME_PAGE);
    await loaded;

    // Get the info of the parent, which is HTTP.
    securityInfo = await browser.browsingContext.currentWindowGlobal.getSecurityInfo();
    is(securityInfo, null, "Found no security info");

    // Get the info of the frame, which is HTTPS.
    securityInfo = await browser.browsingContext
      .getChildren()[0]
      .currentWindowGlobal.getSecurityInfo();
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    ok(securityInfo, "Found some security info");
    ok(securityInfo.succeededCertChain, "Has a succeeded cert chain");
    is(securityInfo.errorCode, 0, "Has no error code");
    is(
      securityInfo.serverCert.commonName,
      "example.com",
      "Has the correct certificate"
    );
  });
});
