const CAT_PREF = "browser.contentblocking.category";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const ST_PREF = "privacy.trackingprotection.socialtracking.enabled";
const STC_PREF = "privacy.socialtracking.block_cookies.enabled";
const FPP_PREF = "privacy.fingerprintingProtection";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);

const l10n = new Localization([
  "browser/siteProtections.ftl",
  "branding/brand.ftl",
]);

registerCleanupFunction(function () {
  Services.prefs.clearUserPref(CAT_PREF);
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  Services.prefs.clearUserPref(CM_PREF);
  Services.prefs.clearUserPref(FP_PREF);
  Services.prefs.clearUserPref(ST_PREF);
  Services.prefs.clearUserPref(STC_PREF);
});

add_task(async function testCookieCategoryLabel() {
  await BrowserTestUtils.withNewTab(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://www.example.com",
    async function () {
      // Ensure the category nodes exist.
      await openProtectionsPanel();
      await closeProtectionsPanel();
      let categoryItem = document.getElementById(
        "protections-popup-category-cookies"
      );
      let categoryLabel = document.getElementById(
        "protections-popup-cookies-category-label"
      );

      Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
      await TestUtils.waitForCondition(
        () => !categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(!categoryItem.classList.contains("blocked"));

      Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT);
      await TestUtils.waitForCondition(
        () => categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(categoryItem.classList.contains("blocked"));
      const blockAllMsg = await l10n.formatValue(
        "content-blocking-cookies-blocking-all-label"
      );
      await TestUtils.waitForCondition(
        () => categoryLabel.textContent == blockAllMsg,
        "The category label has updated correctly"
      );

      Services.prefs.setIntPref(
        TPC_PREF,
        Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
      );
      await TestUtils.waitForCondition(
        () => categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(categoryItem.classList.contains("blocked"));
      const block3rdMsg = await l10n.formatValue(
        "content-blocking-cookies-blocking-third-party-label"
      );
      await TestUtils.waitForCondition(
        () => categoryLabel.textContent == block3rdMsg,
        "The category label has updated correctly"
      );

      Services.prefs.setIntPref(
        TPC_PREF,
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
      );
      await TestUtils.waitForCondition(
        () => categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(categoryItem.classList.contains("blocked"));
      const blockTrackersMsg = await l10n.formatValue(
        "content-blocking-cookies-blocking-trackers-label"
      );
      await TestUtils.waitForCondition(
        () => categoryLabel.textContent == blockTrackersMsg,
        "The category label has updated correctly"
      );

      Services.prefs.setIntPref(
        TPC_PREF,
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
      );
      await TestUtils.waitForCondition(
        () => categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(categoryItem.classList.contains("blocked"));
      await TestUtils.waitForCondition(
        () => categoryLabel.textContent == blockTrackersMsg,
        "The category label has updated correctly"
      );

      Services.prefs.setIntPref(
        TPC_PREF,
        Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
      );
      await TestUtils.waitForCondition(
        () => !categoryItem.classList.contains("blocked"),
        "The category label has updated correctly"
      );
      ok(!categoryItem.classList.contains("blocked"));
    }
  );
});

let categoryEnabledPrefs = [TP_PREF, STC_PREF, TPC_PREF, CM_PREF, FP_PREF];

let detectedStateFlags = [
  Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
  Ci.nsIWebProgressListener.STATE_BLOCKED_SOCIALTRACKING_CONTENT,
  Ci.nsIWebProgressListener.STATE_COOKIES_LOADED,
  Ci.nsIWebProgressListener.STATE_BLOCKED_CRYPTOMINING_CONTENT,
  Ci.nsIWebProgressListener.STATE_BLOCKED_FINGERPRINTING_CONTENT,
];

async function waitForClass(item, className, shouldBePresent = true) {
  await TestUtils.waitForCondition(() => {
    return item.classList.contains(className) == shouldBePresent;
  }, `Target class ${className} should be ${shouldBePresent ? "present" : "not present"} on item ${item.id}`);

  Assert.equal(
    item.classList.contains(className),
    shouldBePresent,
    `item.classList.contains(${className}) is ${shouldBePresent} for ${item.id}`
  );
}

add_task(async function testCategorySections() {
  Services.prefs.setBoolPref(ST_PREF, true);
  Services.prefs.setBoolPref(FPP_PREF, false);

  for (let pref of categoryEnabledPrefs) {
    if (pref == TPC_PREF) {
      Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
    } else {
      Services.prefs.setBoolPref(pref, false);
    }
  }

  await BrowserTestUtils.withNewTab(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://www.example.com",
    async function () {
      // Ensure the category nodes exist.
      await openProtectionsPanel();
      await closeProtectionsPanel();

      let categoryItems = [
        "protections-popup-category-trackers",
        "protections-popup-category-socialblock",
        "protections-popup-category-cookies",
        "protections-popup-category-cryptominers",
        "protections-popup-category-fingerprinters",
      ].map(id => document.getElementById(id));

      for (let item of categoryItems) {
        await waitForClass(item, "notFound");
        await waitForClass(item, "blocked", false);
      }

      // For every item, we enable the category and spoof a content blocking event,
      // and check that .notFound goes away and .blocked is set. Then we disable the
      // category and checks that .blocked goes away, and .notFound is still unset.
      let contentBlockingState = 0;
      for (let i = 0; i < categoryItems.length; i++) {
        let itemToTest = categoryItems[i];
        let enabledPref = categoryEnabledPrefs[i];
        contentBlockingState |= detectedStateFlags[i];
        if (enabledPref == TPC_PREF) {
          Services.prefs.setIntPref(
            TPC_PREF,
            Ci.nsICookieService.BEHAVIOR_REJECT
          );
        } else {
          Services.prefs.setBoolPref(enabledPref, true);
        }
        gProtectionsHandler.onContentBlockingEvent(contentBlockingState);
        gProtectionsHandler.updatePanelForBlockingEvent(contentBlockingState);
        await waitForClass(itemToTest, "notFound", false);
        await waitForClass(itemToTest, "blocked", true);
        if (enabledPref == TPC_PREF) {
          Services.prefs.setIntPref(
            TPC_PREF,
            Ci.nsICookieService.BEHAVIOR_ACCEPT
          );
        } else {
          Services.prefs.setBoolPref(enabledPref, false);
        }
        await waitForClass(itemToTest, "notFound", false);
        await waitForClass(itemToTest, "blocked", false);
      }
    }
  );
});

/**
 * Check that when we open the popup in a new window, the initial state is correct
 * wrt the pref.
 */
add_task(async function testCategorySectionInitial() {
  requestLongerTimeout(3);

  let categoryItems = [
    "protections-popup-category-trackers",
    "protections-popup-category-socialblock",
    "protections-popup-category-cookies",
    "protections-popup-category-cryptominers",
    "protections-popup-category-fingerprinters",
  ];
  for (let i = 0; i < categoryItems.length; i++) {
    for (let shouldBlock of [true, false]) {
      let win = await BrowserTestUtils.openNewBrowserWindow();
      // Open non-about: page so our protections are active.
      await BrowserTestUtils.openNewForegroundTab(
        win.gBrowser,
        "https://example.com/"
      );
      let enabledPref = categoryEnabledPrefs[i];
      let contentBlockingState = detectedStateFlags[i];
      if (enabledPref == TPC_PREF) {
        Services.prefs.setIntPref(
          TPC_PREF,
          shouldBlock
            ? Ci.nsICookieService.BEHAVIOR_REJECT
            : Ci.nsICookieService.BEHAVIOR_ACCEPT
        );
      } else {
        Services.prefs.setBoolPref(enabledPref, shouldBlock);
      }
      win.gProtectionsHandler.onContentBlockingEvent(contentBlockingState);
      await openProtectionsPanel(false, win);
      let categoryItem = win.document.getElementById(categoryItems[i]);
      let expectedFound = true;
      // Accepting cookies outright won't mark this as found.
      if (i == 2 && !shouldBlock) {
        // See bug 1653019
        expectedFound = false;
      }
      is(
        categoryItem.classList.contains("notFound"),
        !expectedFound,
        `Should have found ${categoryItems[i]} when it was ${
          shouldBlock ? "blocked" : "allowed"
        }`
      );
      is(
        categoryItem.classList.contains("blocked"),
        shouldBlock,
        `Should ${shouldBlock ? "have blocked" : "not have blocked"} ${
          categoryItems[i]
        }`
      );
      await closeProtectionsPanel(win);
      await BrowserTestUtils.closeWindow(win);
    }
  }
});
