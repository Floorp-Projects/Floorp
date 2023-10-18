/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

let TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let TEST_PATH_AUTH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

const CROSS_DOMAIN_URL = TEST_PATH + "redirect-crossDomain.html";

const AUTH_URL = TEST_PATH_AUTH + "auth-route.sjs";

/**
 * Opens a new tab with a url that redirects us cross domain
 * tests that auth anti-spoofing mechanisms cover url copy while prompt is open
 *
 */
async function trigger401AndHandle() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.authPromptSpoofingProtection", true]],
  });
  let dialogShown = waitForDialogAndCopyURL();
  await BrowserTestUtils.withNewTab(CROSS_DOMAIN_URL, async function () {
    await dialogShown;
  });
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
      resolve
    );
  });
}

async function waitForDialogAndCopyURL() {
  await TestUtils.topicObserved("common-dialog-loaded");
  let dialog = gBrowser.getTabDialogBox(gBrowser.selectedBrowser)
    ._tabDialogManager._topDialog;
  let dialogDocument = dialog._frame.contentDocument;

  //select the whole URL
  gURLBar.focus();
  await SimpleTest.promiseClipboardChange(AUTH_URL, () => {
    Assert.equal(
      gURLBar.value,
      UrlbarTestUtils.trimURL(AUTH_URL),
      "url bar copy value set"
    );
    gURLBar.select();
    goDoCommand("cmd_copy");
  });

  // select only part of the URL
  gURLBar.focus();
  let endOfSelectionRange =
    UrlbarTestUtils.trimURL(AUTH_URL).indexOf("/auth-route.sjs");

  let isProtocolTrimmed = AUTH_URL.startsWith(
    UrlbarTestUtils.getTrimmedProtocolWithSlashes()
  );
  await SimpleTest.promiseClipboardChange(
    AUTH_URL.substring(
      0,
      endOfSelectionRange +
        (isProtocolTrimmed
          ? UrlbarTestUtils.getTrimmedProtocolWithSlashes().length
          : 0)
    ),
    () => {
      Assert.equal(
        gURLBar.value,
        UrlbarTestUtils.trimURL(AUTH_URL),
        "url bar copy value set"
      );
      gURLBar.selectionStart = 0;
      gURLBar.selectionEnd = endOfSelectionRange;
      goDoCommand("cmd_copy");
    }
  );
  let onDialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );
  dialogDocument.getElementById("commonDialog").cancelDialog();

  await onDialogClosed;
  Assert.equal(
    window.gURLBar.value,
    UrlbarTestUtils.trimURL(CROSS_DOMAIN_URL),
    "No location is provided by the prompt"
  );

  //select the whole URL after URL is reset to normal
  gURLBar.focus();
  await SimpleTest.promiseClipboardChange(CROSS_DOMAIN_URL, () => {
    Assert.equal(
      gURLBar.value,
      UrlbarTestUtils.trimURL(CROSS_DOMAIN_URL),
      "url bar copy value set"
    );
    gURLBar.select();
    goDoCommand("cmd_copy");
  });
}

/**
 * Tests that the 401 auth spoofing mechanisms covers the url bar copy action properly,
 * canceling the prompt
 */
add_task(async function testUrlCopy() {
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.trimHttps", false],
      ["browser.urlbar.trimURLs", true],
    ],
  });
  await trigger401AndHandle();
  SpecialPowers.popPrefEnv();

  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.trimHttps", true],
      ["browser.urlbar.trimURLs", true],
    ],
  });
  await trigger401AndHandle();
});
