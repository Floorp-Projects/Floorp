/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "mixed_active.html";

/**
 * Tests a given url.
 * The de-emphasized parts must be wrapped in "<" and ">" chars.
 *
 * @param {string} urlFormatString The URL to test.
 * @param {string} [clobberedURLString] Normally the URL is de-emphasized
 *        in-place, thus it's enough to pass aExpected. Though, in some cases
 *        the formatter may decide to replace the URL with a fixed one, because
 *        it can't properly guess a host. In that case clobberedURLString is
 *        the expected de-emphasized value.
 */
async function testVal(urlFormatString, clobberedURLString = null) {
  let str = urlFormatString.replace(/[<>]/g, "");

  info("Setting the value property directly");
  gURLBar.value = str;
  gBrowser.selectedBrowser.focus();
  UrlbarTestUtils.checkFormatting(window, urlFormatString, {
    clobberedURLString,
    selectionType: Ci.nsISelectionController.SELECTION_URLSTRIKEOUT,
  });
}

add_task(async function test_strikeout_on_no_https_trimming() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.trimHttps", false],
      ["security.mixed_content.block_active_content", false],
    ],
  });
  await BrowserTestUtils.withNewTab(TEST_URL, function () {
    testVal("<https>://example.com/mixed_active.html");
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_no_strikeout_on_https_trimming() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.trimHttps", true],
      ["security.mixed_content.block_active_content", false],
    ],
  });
  await BrowserTestUtils.withNewTab(TEST_URL, function () {
    testVal(
      "https://example.com/mixed_active.html",
      "example.com/mixed_active.html"
    );
  });
  await SpecialPowers.popPrefEnv();
});
