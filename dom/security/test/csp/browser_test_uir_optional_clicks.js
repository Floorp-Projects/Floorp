"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_PATH_HTTPS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
  await BrowserTestUtils.withNewTab(
    TEST_PATH_HTTPS + "file_csp_meta_uir.html",
    async function (browser) {
      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
      BrowserTestUtils.synthesizeMouse(
        "#mylink",
        2,
        2,
        { accelKey: true },
        browser
      );
      let tab = await newTabPromise;
      is(
        tab.linkedBrowser.currentURI.scheme,
        "https",
        "Should have opened https page."
      );
      BrowserTestUtils.removeTab(tab);
    }
  );
});
