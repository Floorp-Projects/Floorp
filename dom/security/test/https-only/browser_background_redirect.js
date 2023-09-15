"use strict";
/* Description of the test:
 * We load a page which gets upgraded to https. HTTPS-Only Mode then
 * sends an 'http' background request which we redirect (using the
 * web extension API) to 'same-origin' https. We ensure the HTTPS-Only
 * Error page does not occur, but we https page gets loaded.
 */

let extension = null;

add_task(async function test_https_only_background_request_redirect() {
  // A longer timeout is necessary for this test since we have to wait
  // at least 3 seconds for the https-only background request to happen.
  requestLongerTimeout(10);

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/*"],
    },
    background() {
      let { browser } = this;
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          if (details.url === "http://example.com/") {
            browser.test.sendMessage("redir-handled");
            let redirectUrl = "https://example.com/";
            return { redirectUrl };
          }
          return undefined;
        },
        { urls: ["http://example.com/*"] },
        ["blocking"]
      );
    },
  });

  await extension.startup();

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(
      browser,
      "http://example.com/browser/dom/security/test/https-only/file_background_redirect.sjs?start"
    );

    await extension.awaitMessage("redir-handled");

    await loaded;

    await SpecialPowers.spawn(browser, [], async function () {
      let innerHTML = content.document.body.innerHTML;
      ok(
        innerHTML.includes("Test Page for Bug 1683015 loaded"),
        "No https-only error page"
      );
    });
  });

  await extension.unload();
});
