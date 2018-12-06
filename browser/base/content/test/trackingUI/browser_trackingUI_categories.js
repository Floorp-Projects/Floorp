const CAT_PREF = "browser.contentblocking.category";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const TT_PREF = "urlclassifier.trackingTable";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  Services.prefs.clearUserPref(TT_PREF);
  Services.prefs.clearUserPref(CAT_PREF);
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
  SpecialPowers.pushPrefEnv({set: [["browser.contentblocking.control-center.ui.showAllowedLabels", true]]});

  await BrowserTestUtils.withNewTab("http://www.example.com", async function() {
    let categoryLabel =
      document.getElementById("identity-popup-content-blocking-tracking-protection-state-label");

    Services.prefs.setBoolPref(TP_PREF, true);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.trackers.blocked.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.trackers.blocked.label"));

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
      gNavigatorBundle.getString("contentBlocking.cookies.allBlocked.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.allBlocked.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.3rdPartyBlocked.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.3rdPartyBlocked.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.trackersBlocked.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.trackersBlocked.label"));

    Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN);
    await TestUtils.waitForCondition(() => categoryLabel.textContent ==
      gNavigatorBundle.getString("contentBlocking.cookies.unvisitedBlocked.label"),
      "The category label has updated correctly");
    is(categoryLabel.textContent, gNavigatorBundle.getString("contentBlocking.cookies.unvisitedBlocked.label"));
  });
});
