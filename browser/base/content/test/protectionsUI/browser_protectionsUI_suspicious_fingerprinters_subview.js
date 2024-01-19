/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

/**
 * Bug 1863280 - Testing the fingerprinting category of the protection panel
 *               shows the suspicious fingerpinter domain if the fingerprinting
 *               protection is enabled
 */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  ""
);

const TEST_DOMAIN = "https://www.example.com";
const TEST_3RD_DOMAIN = "https://example.org";

const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "benignPage.html";
const TEST_3RD_CANVAS_FP_PAGE =
  TEST_3RD_DOMAIN + TEST_PATH + "canvas-fingerprinter.html";
const TEST_3RD_FONT_FP_PAGE =
  TEST_3RD_DOMAIN + TEST_PATH + "font-fingerprinter.html";

const FINGERPRINT_BLOCKING_PREF =
  "privacy.trackingprotection.fingerprinting.enabled";
const FINGERPRINT_PROTECTION_PREF = "privacy.fingerprintingProtection";
const FINGERPRINT_PROTECTION_PBM_PREF =
  "privacy.fingerprintingProtection.pbmode";

/**
 * A helper function to check whether or not an element has "notFound" class.
 *
 * @param {String} id The id of the testing element.
 * @returns {Boolean} true when the element has "notFound" class.
 */
function notFound(id) {
  return document.getElementById(id).classList.contains("notFound");
}

/**
 * A helper function to wait until the suspicious fingerprinting content
 * blocking event is dispatched.
 *
 * @param {Window} win The window to listen content blocking events.
 * @returns {Promise} A promise that resolves when the event files.
 */
async function waitForSuspiciousFingerprintingEvent(win) {
  return new Promise(resolve => {
    let listener = {
      onContentBlockingEvent(webProgress, request, event) {
        info(`Received onContentBlockingEvent event: ${event}`);
        if (
          event &
          Ci.nsIWebProgressListener.STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING
        ) {
          win.gBrowser.removeProgressListener(listener);
          resolve();
        }
      },
    };
    win.gBrowser.addProgressListener(listener);
  });
}

/**
 * A helper function that opens a tests page that embeds iframes with given
 * urls.
 *
 * @param {string[]} urls An array of urls that will be embedded in the test page.
 * @param {boolean} usePrivateWin true to indicate testing in a private window.
 * @param {Function} testFn The test function that will be called after the
 * iframes have been loaded.
 */
async function openTestPage(urls, usePrivateWin, testFn) {
  let win = window;

  if (usePrivateWin) {
    win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  }

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: TEST_PAGE },
    async browser => {
      info("Loading all iframes");

      for (const url of urls) {
        await SpecialPowers.spawn(browser, [url], async testUrl => {
          await new content.Promise(resolve => {
            let ifr = content.document.createElement("iframe");
            ifr.onload = resolve;

            content.document.body.appendChild(ifr);
            ifr.src = testUrl;
          });
        });
      }

      info("Running the test");
      await testFn(win, browser);
    }
  );

  if (usePrivateWin) {
    await BrowserTestUtils.closeWindow(win);
  }
}

/**
 * A testing function verifies that fingerprinting category is not shown.
 *
 * @param {*} win The window to run the test.
 */
async function testCategoryNotShown(win) {
  await openProtectionsPanel(false, win);

  let categoryItem = win.document.getElementById(
    "protections-popup-category-fingerprinters"
  );

  // The fingerprinting category should have the 'notFound' class to indicate
  // that no fingerprinter was found in the page.
  ok(
    notFound("protections-popup-category-fingerprinters"),
    "Fingerprinting category is not found"
  );

  ok(
    !BrowserTestUtils.isVisible(categoryItem),
    "Fingerprinting category item is not visible"
  );

  await closeProtectionsPanel(win);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [[FINGERPRINT_BLOCKING_PREF, true]],
  });
});

// Verify that fingerprinting category is not shown if fingerprinting protection
// is disabled.
add_task(async function testFPPDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [[FINGERPRINT_PROTECTION_PREF, false]],
  });

  await openTestPage(
    [TEST_3RD_CANVAS_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
    false,
    testCategoryNotShown
  );
});

// Verify that fingerprinting category is not shown if no fingerprinting
// activity is detected.
add_task(async function testFPPEnabledWithoutFingerprintingActivity() {
  await SpecialPowers.pushPrefEnv({
    set: [[FINGERPRINT_PROTECTION_PREF, true]],
  });

  // Test the case where the page doesn't load any fingerprinter.
  await openTestPage([], false, testCategoryNotShown);

  // Test the case where the page loads only one fingerprinter. We don't treat
  // this case as suspicious fingerprinting.
  await openTestPage([TEST_3RD_FONT_FP_PAGE], false, testCategoryNotShown);

  // Test the case where the page loads the same fingerprinter multiple times.
  // We don't treat this case as suspicious fingerprinting.
  await openTestPage(
    [TEST_3RD_FONT_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
    false,
    testCategoryNotShown
  );
});

// Verify that fingerprinting category is not shown if no fingerprinting
// activity is detected.
add_task(
  async function testFPPEnabledWithoutSuspiciousFingerprintingActivity() {
    await SpecialPowers.pushPrefEnv({
      set: [[FINGERPRINT_PROTECTION_PREF, true]],
    });

    // Test the case where the page doesn't load any fingerprinter.
    await openTestPage([], false, testCategoryNotShown);

    // Test the case where the page loads only one fingerprinter. We don't treat
    // this case as suspicious fingerprinting.
    await openTestPage([TEST_3RD_FONT_FP_PAGE], false, testCategoryNotShown);

    // Test the case where the page loads the same fingerprinter multiple times.
    // We don't treat this case as suspicious fingerprinting.
    await openTestPage(
      [TEST_3RD_FONT_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
      false,
      testCategoryNotShown
    );
  }
);

// Verify that fingerprinting category is properly shown and the fingerprinting
// subview displays the origin of the suspicious fingerprinter.
add_task(async function testFingerprintingSubview() {
  await SpecialPowers.pushPrefEnv({
    set: [[FINGERPRINT_PROTECTION_PREF, true]],
  });

  await openTestPage(
    [TEST_3RD_CANVAS_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
    false,
    async (win, _) => {
      await openProtectionsPanel(false, win);

      let categoryItem = win.document.getElementById(
        "protections-popup-category-fingerprinters"
      );

      // Explicitly waiting for the category item becoming visible.
      await BrowserTestUtils.waitForMutationCondition(categoryItem, {}, () =>
        BrowserTestUtils.isVisible(categoryItem)
      );

      ok(
        BrowserTestUtils.isVisible(categoryItem),
        "Fingerprinting category item is visible"
      );

      // Click the fingerprinting category and wait until the fingerprinter view is
      // shown.
      let fingerprintersView = win.document.getElementById(
        "protections-popup-fingerprintersView"
      );
      let viewShown = BrowserTestUtils.waitForEvent(
        fingerprintersView,
        "ViewShown"
      );
      categoryItem.click();
      await viewShown;

      ok(true, "Fingerprinter view was shown");

      // Ensure the fingerprinter is listed on the tracker list.
      let listItems = Array.from(
        fingerprintersView.querySelectorAll(".protections-popup-list-item")
      );
      is(listItems.length, 1, "We have 1 fingerprinter in the list");

      let listItem = listItems.find(
        item => item.querySelector("label").value == "https://example.org"
      );
      ok(listItem, "Has an item for example.org");
      ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

      // Back to the popup main view.
      let mainView = win.document.getElementById("protections-popup-mainView");
      viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
      let backButton = fingerprintersView.querySelector(".subviewbutton-back");
      backButton.click();
      await viewShown;

      ok(true, "Main view was shown");

      await closeProtectionsPanel(win);
    }
  );
});

// Verify the case where the fingerprinting protection is only enabled in PBM.
add_task(async function testFingerprintingSubviewInPBM() {
  // Only enabled fingerprinting protection in PBM.
  await SpecialPowers.pushPrefEnv({
    set: [
      [FINGERPRINT_PROTECTION_PREF, false],
      [FINGERPRINT_PROTECTION_PBM_PREF, true],
    ],
  });

  // Verify that fingerprinting category isn't shown on a normal window.
  await openTestPage(
    [TEST_3RD_CANVAS_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
    false,
    testCategoryNotShown
  );

  // Verify that fingerprinting category is shown on a private window.
  await openTestPage(
    [TEST_3RD_CANVAS_FP_PAGE, TEST_3RD_FONT_FP_PAGE],
    true,
    async (win, _) => {
      await openProtectionsPanel(false, win);

      let categoryItem = win.document.getElementById(
        "protections-popup-category-fingerprinters"
      );

      // Explicitly waiting for the category item becoming visible.
      await BrowserTestUtils.waitForMutationCondition(categoryItem, {}, () =>
        BrowserTestUtils.isVisible(categoryItem)
      );

      ok(
        BrowserTestUtils.isVisible(categoryItem),
        "Fingerprinting category item is visible"
      );

      // Click the fingerprinting category and wait until the fingerprinter view is
      // shown.
      let fingerprintersView = win.document.getElementById(
        "protections-popup-fingerprintersView"
      );
      let viewShown = BrowserTestUtils.waitForEvent(
        fingerprintersView,
        "ViewShown"
      );
      categoryItem.click();
      await viewShown;

      ok(true, "Fingerprinter view was shown");

      // Ensure the fingerprinter is listed on the tracker list.
      let listItems = Array.from(
        fingerprintersView.querySelectorAll(".protections-popup-list-item")
      );
      is(listItems.length, 1, "We have 1 fingerprinter in the list");

      let listItem = listItems.find(
        item => item.querySelector("label").value == "https://example.org"
      );
      ok(listItem, "Has an item for example.org");
      ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

      // Back to the popup main view.
      let mainView = win.document.getElementById("protections-popup-mainView");
      viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
      let backButton = fingerprintersView.querySelector(".subviewbutton-back");
      backButton.click();
      await viewShown;

      ok(true, "Main view was shown");

      await closeProtectionsPanel(win);
    }
  );
});

// Verify that fingerprinting category will be shown after loading
// fingerprinters.
add_task(async function testDynamicallyLoadFingerprinter() {
  await SpecialPowers.pushPrefEnv({
    set: [[FINGERPRINT_PROTECTION_PREF, true]],
  });

  await openTestPage([TEST_3RD_FONT_FP_PAGE], false, async (win, browser) => {
    await openProtectionsPanel(false, win);

    let categoryItem = win.document.getElementById(
      "protections-popup-category-fingerprinters"
    );

    // The fingerprinting category should have the 'notFound' class to indicate
    // that no suspicious fingerprinter was found in the page.
    ok(
      notFound("protections-popup-category-fingerprinters"),
      "Fingerprinting category is not found"
    );

    ok(
      !BrowserTestUtils.isVisible(categoryItem),
      "Fingerprinting category item is not visible"
    );

    // Add an iframe that triggers suspicious fingerprinting and wait until the
    // content event files.

    let contentBlockingEventPromise = waitForSuspiciousFingerprintingEvent(win);
    await SpecialPowers.spawn(browser, [TEST_3RD_CANVAS_FP_PAGE], test_url => {
      let ifr = content.document.createElement("iframe");

      content.document.body.appendChild(ifr);
      ifr.src = test_url;
    });
    await contentBlockingEventPromise;

    // Click the fingerprinting category and wait until the fingerprinter view
    // is shown.
    let fingerprintersView = win.document.getElementById(
      "protections-popup-fingerprintersView"
    );
    let viewShown = BrowserTestUtils.waitForEvent(
      fingerprintersView,
      "ViewShown"
    );
    categoryItem.click();
    await viewShown;

    ok(true, "Fingerprinter view was shown");

    // Ensure the fingerprinter is listed on the tracker list.
    let listItems = Array.from(
      fingerprintersView.querySelectorAll(".protections-popup-list-item")
    );
    is(listItems.length, 1, "We have 1 fingerprinter in the list");

    let listItem = listItems.find(
      item => item.querySelector("label").value == "https://example.org"
    );
    ok(listItem, "Has an item for example.org");
    ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

    // Back to the popup main view.
    let mainView = win.document.getElementById("protections-popup-mainView");
    viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    let backButton = fingerprintersView.querySelector(".subviewbutton-back");
    backButton.click();
    await viewShown;

    ok(true, "Main view was shown");

    await closeProtectionsPanel(win);
  });
});
