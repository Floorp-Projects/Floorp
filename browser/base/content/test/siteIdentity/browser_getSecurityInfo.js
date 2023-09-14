/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;
const MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT = MOZILLA_PKIX_ERROR_BASE + 14;

add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.waitForErrorPage(browser);
    BrowserTestUtils.startLoadingURIString(
      browser,
      "https://self-signed.example.com"
    );
    await loaded;
    let securityInfo = gBrowser.securityUI.secInfo;
    ok(!securityInfo, "Found no security info");

    loaded = BrowserTestUtils.browserLoaded(browser);
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    BrowserTestUtils.startLoadingURIString(browser, "http://example.com");
    await loaded;
    securityInfo = gBrowser.securityUI.secInfo;
    ok(!securityInfo, "Found no security info");

    loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.startLoadingURIString(browser, "https://example.com");
    await loaded;
    securityInfo = gBrowser.securityUI.secInfo;
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
