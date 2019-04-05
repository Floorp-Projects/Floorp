const CAT_PREF = "browser.contentblocking.category";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  Services.prefs.clearUserPref(CAT_PREF);
  Services.prefs.clearUserPref(CM_PREF);
  Services.prefs.clearUserPref(FP_PREF);
});

add_task(async function testCategoryLabelsInControlPanel() {
  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    await openIdentityPopup();

    let preferencesButton = document.getElementById("tracking-protection-preferences-button");
    ok(preferencesButton.label, "The preferencesButton label exists");

    Services.prefs.setStringPref(CAT_PREF, "strict");
    await TestUtils.waitForCondition(() => preferencesButton.label ==
      gNavigatorBundle.getString("contentBlocking.category.strict"));
    is(preferencesButton.label, gNavigatorBundle.getString("contentBlocking.category.strict"),
      "The preferencesButton label has been changed to strict");

    Services.prefs.setStringPref(CAT_PREF, "standard");
    await TestUtils.waitForCondition(() => preferencesButton.label ==
      gNavigatorBundle.getString("contentBlocking.category.standard"));
    is(preferencesButton.label, gNavigatorBundle.getString("contentBlocking.category.standard"),
      "The preferencesButton label has been changed to standard");

    Services.prefs.setStringPref(CAT_PREF, "custom");
    await TestUtils.waitForCondition(() => preferencesButton.label ==
      gNavigatorBundle.getString("contentBlocking.category.custom"));
    is(preferencesButton.label, gNavigatorBundle.getString("contentBlocking.category.custom"),
      "The preferencesButton label has been changed to custom");
  });
});

add_task(async function testCategoryLabelsInAppMenu() {
  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    let cuiTestUtils = new CustomizableUITestUtils(window);
    await cuiTestUtils.openMainMenu();

    let appMenuCategoryLabel = document.getElementById("appMenu-tp-category");
    ok(appMenuCategoryLabel.value, "The appMenuCategory label exists");

    Services.prefs.setStringPref(CAT_PREF, "strict");
    await TestUtils.waitForCondition(() => appMenuCategoryLabel.value ==
      gNavigatorBundle.getString("contentBlocking.category.strict"));
    is(appMenuCategoryLabel.value, gNavigatorBundle.getString("contentBlocking.category.strict"),
      "The appMenuCategory label has been changed to strict");

    Services.prefs.setStringPref(CAT_PREF, "standard");
    await TestUtils.waitForCondition(() => appMenuCategoryLabel.value ==
      gNavigatorBundle.getString("contentBlocking.category.standard"));
    is(appMenuCategoryLabel.value, gNavigatorBundle.getString("contentBlocking.category.standard"),
      "The appMenuCategory label has been changed to standard");

    Services.prefs.setStringPref(CAT_PREF, "custom");
    await TestUtils.waitForCondition(() => appMenuCategoryLabel.value ==
      gNavigatorBundle.getString("contentBlocking.category.custom"));
    is(appMenuCategoryLabel.value, gNavigatorBundle.getString("contentBlocking.category.custom"),
      "The appMenuCategory label has been changed to custom");
  });
});

add_task(async function testSubcategoryLabels() {
  SpecialPowers.pushPrefEnv({set: [
    ["browser.contentblocking.control-center.ui.showAllowedLabels", true],
    ["browser.contentblocking.control-center.ui.showBlockedLabels", true],
  ]});

  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    let categoryLabel =
      document.getElementById("identity-popup-content-blocking-tracking-protection-state-label");

    Services.prefs.setBoolPref(TP_PREF, true);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.trackers.blocking.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.trackers.blocking.label"));

    Services.prefs.setBoolPref(TP_PREF, false);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.trackers.allowed.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.trackers.allowed.label"));

    categoryLabel =
      document.getElementById("identity-popup-content-blocking-cookies-state-label");

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.allowed.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.allowed.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.blockingAll.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.blockingAll.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.blocking3rdParty.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.blocking3rdParty.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.blockingTrackers.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.blockingTrackers.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.blockingUnvisited.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.blockingUnvisited.label"));

    categoryLabel =
      document.getElementById("identity-popup-content-blocking-fingerprinters-state-label");

    Services.prefs.setBoolPref(FP_PREF, true);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.fingerprinters.blocking.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.fingerprinters.blocking.label"));

    Services.prefs.setBoolPref(FP_PREF, false);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.fingerprinters.allowed.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.fingerprinters.allowed.label"));

    categoryLabel =
      document.getElementById("identity-popup-content-blocking-cryptominers-state-label");

    Services.prefs.setBoolPref(CM_PREF, true);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cryptominers.blocking.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cryptominers.blocking.label"));

    Services.prefs.setBoolPref(CM_PREF, false);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cryptominers.allowed.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cryptominers.allowed.label"));
  });
});
