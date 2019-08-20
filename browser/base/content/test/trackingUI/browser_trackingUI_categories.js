const CAT_PREF = "browser.contentblocking.category";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";

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
});

add_task(async function testSubcategoryLabels() {
  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    let categoryItem = document.getElementById(
      "protections-popup-category-tracking-protection"
    );

    Services.prefs.setBoolPref(TP_PREF, true);
    await TestUtils.waitForCondition(
      () => categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(categoryItem.classList.contains("blocked"));

    Services.prefs.setBoolPref(TP_PREF, false);
    await TestUtils.waitForCondition(
      () => !categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(!categoryItem.classList.contains("blocked"));

    categoryItem = document.getElementById(
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

    categoryItem = document.getElementById(
      "protections-popup-category-fingerprinters"
    );

    Services.prefs.setBoolPref(FP_PREF, true);
    await TestUtils.waitForCondition(
      () => categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(categoryItem.classList.contains("blocked"));

    Services.prefs.setBoolPref(FP_PREF, false);
    await TestUtils.waitForCondition(
      () => !categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(!categoryItem.classList.contains("blocked"));

    categoryItem = document.getElementById(
      "protections-popup-category-cryptominers"
    );

    Services.prefs.setBoolPref(CM_PREF, true);
    await TestUtils.waitForCondition(
      () => categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(categoryItem.classList.contains("blocked"));

    Services.prefs.setBoolPref(CM_PREF, false);
    await TestUtils.waitForCondition(
      () => !categoryItem.classList.contains("blocked"),
      "The category item has updated correctly"
    );
    ok(!categoryItem.classList.contains("blocked"));
  });
});
