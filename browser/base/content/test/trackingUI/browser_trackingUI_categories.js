const CAT_PREF = "browser.contentblocking.category";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const ST_PREF = "privacy.socialtracking.block_cookies.enabled";

ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm",
  this
);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  Services.prefs.clearUserPref(CAT_PREF);
  Services.prefs.clearUserPref(CM_PREF);
  Services.prefs.clearUserPref(FP_PREF);
  Services.prefs.clearUserPref(ST_PREF);
});

add_task(async function testCookieCategoryLabels() {
  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    let categoryItem = document.getElementById(
      "protections-popup-category-cookies"
    );
    let categoryLabelDisabled = document.getElementById(
      "protections-popup-cookies-category-label-disabled"
    );
    let categoryLabelEnabled = document.getElementById(
      "protections-popup-cookies-category-label-enabled"
    );

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
    await TestUtils.waitForCondition(
      () =>
        !categoryItem.classList.contains("blocked") &&
        !categoryLabelDisabled.hidden &&
        categoryLabelEnabled.hidden,
      "The category label has updated correctly"
    );
    ok(
      !categoryItem.classList.contains("blocked") &&
        !categoryLabelDisabled.hidden &&
        categoryLabelEnabled.hidden
    );

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT);
    await TestUtils.waitForCondition(
      () =>
        categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden,
      "The category label has updated correctly"
    );
    ok(
      categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden
    );
    await TestUtils.waitForCondition(
      () =>
        categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString(
          "contentBlocking.cookies.blockingAll2.label"
        ),
      "The category label has updated correctly"
    );
    ok(
      categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString("contentBlocking.cookies.blockingAll2.label")
    );

    Services.prefs.setIntPref(
      TPC_PREF,
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
    );
    await TestUtils.waitForCondition(
      () =>
        categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden,
      "The category label has updated correctly"
    );
    ok(
      categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden
    );
    await TestUtils.waitForCondition(
      () =>
        categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString(
          "contentBlocking.cookies.blocking3rdParty2.label"
        ),
      "The category label has updated correctly"
    );
    ok(
      categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString(
          "contentBlocking.cookies.blocking3rdParty2.label"
        )
    );

    Services.prefs.setIntPref(
      TPC_PREF,
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
    );
    await TestUtils.waitForCondition(
      () =>
        categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden,
      "The category label has updated correctly"
    );
    ok(
      categoryItem.classList.contains("blocked") &&
        categoryLabelDisabled.hidden &&
        !categoryLabelEnabled.hidden
    );
    await TestUtils.waitForCondition(
      () =>
        categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString(
          "contentBlocking.cookies.blockingTrackers3.label"
        ),
      "The category label has updated correctly"
    );
    ok(
      categoryLabelEnabled.textContent ==
        gNavigatorBundle.getString(
          "contentBlocking.cookies.blockingTrackers3.label"
        )
    );

    Services.prefs.setIntPref(
      TPC_PREF,
      Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
    );
    await TestUtils.waitForCondition(
      () =>
        !categoryItem.classList.contains("blocked") &&
        !categoryLabelDisabled.hidden &&
        categoryLabelEnabled.hidden,
      "The category label has updated correctly"
    );
    ok(
      !categoryItem.classList.contains("blocked") &&
        !categoryLabelDisabled.hidden &&
        categoryLabelEnabled.hidden
    );
  });
});

let categoryItems = [
  "protections-popup-category-tracking-protection",
  "protections-popup-category-socialblock",
  "protections-popup-category-cookies",
  "protections-popup-category-cryptominers",
  "protections-popup-category-fingerprinters",
].map(id => document.getElementById(id));

let categoryEnabledPrefs = [TP_PREF, ST_PREF, TPC_PREF, CM_PREF, FP_PREF];

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

  ok(
    item.classList.contains(className) == shouldBePresent,
    `item.classList.contains(${className}) is ${shouldBePresent} for ${item.id}`
  );
}

add_task(async function testCategorySections() {
  for (let pref of categoryEnabledPrefs) {
    if (pref == TPC_PREF) {
      Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
    } else {
      Services.prefs.setBoolPref(pref, false);
    }
  }

  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
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
  });
});
