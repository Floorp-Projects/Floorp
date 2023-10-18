/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_stripAuthCredentials() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "overlink_test.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], function () {
        content.document.getElementById("link").focus();
      });

      await TestUtils.waitForCondition(
        () =>
          XULBrowserWindow.overLink ==
          UrlbarTestUtils.trimURL("https://example.com"),
        "Overlink should be missing auth credentials"
      );

      ok(true, "Test successful");
    }
  );
});
