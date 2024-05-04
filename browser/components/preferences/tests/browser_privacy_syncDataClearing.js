/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * With no custom cleaning categories set and sanitizeOnShutdown disabled,
 * the checkboxes "alwaysClear" and "deleteOnClose" should share the same state.
 * The state of the cleaning categories cookies, cache and offlineApps should be in the state of the "deleteOnClose" box.
 */
add_task(async function test_syncWithoutCustomPrefs() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  let document = gBrowser.contentDocument;
  let deleteOnCloseBox = document.getElementById("deleteOnClose");
  let alwaysClearBox = document.getElementById("alwaysClear");

  ok(!deleteOnCloseBox.checked, "DeleteOnClose initial state is deselected");
  ok(!alwaysClearBox.checked, "AlwaysClear initial state is deselected");

  deleteOnCloseBox.click();

  ok(deleteOnCloseBox.checked, "DeleteOnClose is selected");
  is(
    deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "DeleteOnClose sets alwaysClear in the same state, selected"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    "Cookie cleaning pref is set"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cache"),
    "Cache cleaning pref is set"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    "OfflineApps cleaning pref is set"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.downloads"),
    "Downloads cleaning pref is not set"
  );

  deleteOnCloseBox.click();

  ok(!deleteOnCloseBox.checked, "DeleteOnClose is deselected");
  is(
    deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "DeleteOnclose sets alwaysClear in the same state, deselected"
  );

  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    "Cookie cleaning pref is reset"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.cache"),
    "Cache cleaning pref is reset"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    "OfflineApps cleaning pref is reset"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.downloads"),
    "Downloads cleaning pref is not set"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Services.prefs.clearUserPref("privacy.clearOnShutdown.downloads");
  Services.prefs.clearUserPref("privacy.clearOnShutdown.offlineApps");
  Services.prefs.clearUserPref("privacy.clearOnShutdown.cache");
  Services.prefs.clearUserPref("privacy.clearOnShutdown.cookies");
  Services.prefs.clearUserPref("privacy.sanitize.sanitizeOnShutdown");
});

/*
 * With custom cleaning category already set and SanitizeOnShutdown enabled,
 * deselecting "deleteOnClose" should not change the state of "alwaysClear".
 * The state of the cleaning categories cookies, cache and offlineApps should be in the state of the "deleteOnClose" box.
 */
add_task(async function test_syncWithCustomPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.history", true],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  let document = gBrowser.contentDocument;
  let deleteOnCloseBox = document.getElementById("deleteOnClose");
  let alwaysClearBox = document.getElementById("alwaysClear");

  ok(!deleteOnCloseBox.checked, "DeleteOnClose initial state is deselected");
  ok(alwaysClearBox.checked, "AlwaysClear initial state is selected");

  deleteOnCloseBox.click();

  ok(deleteOnCloseBox.checked, "DeleteOnClose is selected");
  is(
    deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "AlwaysClear and deleteOnClose are in the same state, selected"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.history"),
    "History cleaning pref is still set"
  );

  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    "Cookie cleaning pref is set"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cache"),
    "Cache cleaning pref is set"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    "OfflineApps cleaning pref is set"
  );

  deleteOnCloseBox.click();

  ok(!deleteOnCloseBox.checked, "DeleteOnClose is deselected");
  is(
    !deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "AlwaysClear is not synced with deleteOnClose, only deleteOnClose is deselected"
  );

  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    "Cookie cleaning pref is reset"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.cache"),
    "Cache cleaning pref is reset"
  );
  ok(
    !Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    "OfflineApps cleaning pref is reset"
  );
  ok(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.history"),
    "History cleaning pref is still set"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

/*
 * Setting/resetting cleaning prefs for cookies, cache, offline apps
 * and selecting/deselecting the "alwaysClear" Box, also selects/deselects
 * the "deleteOnClose" box.
 */

add_task(async function test_syncWithCustomPrefs() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  let document = gBrowser.contentDocument;
  let deleteOnCloseBox = document.getElementById("deleteOnClose");
  let alwaysClearBox = document.getElementById("alwaysClear");

  ok(!deleteOnCloseBox.checked, "DeleteOnClose initial state is deselected");
  ok(!alwaysClearBox.checked, "AlwaysClear initial state is deselected");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", true],
      ["privacy.clearOnShutdown.cache", true],
      ["privacy.clearOnShutdown.offlineApps", true],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });

  ok(alwaysClearBox.checked, "AlwaysClear is selected");
  is(
    deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "AlwaysClear and deleteOnClose are in the same state, selected"
  );

  alwaysClearBox.click();

  ok(!alwaysClearBox.checked, "AlwaysClear is deselected");
  is(
    deleteOnCloseBox.checked,
    alwaysClearBox.checked,
    "AlwaysClear and deleteOnClose are in the same state, deselected"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

/*
 * On loading the page, the ClearOnClose box should be set according to the pref selection
 */
add_task(async function test_initialState() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", true],
      ["privacy.clearOnShutdown.cache", true],
      ["privacy.clearOnShutdown.offlineApps", true],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  let document = gBrowser.contentDocument;
  let deleteOnCloseBox = document.getElementById("deleteOnClose");

  ok(
    deleteOnCloseBox.checked,
    "DeleteOnClose is set accordingly to the prefs, selected"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", false],
      ["privacy.clearOnShutdown.cache", false],
      ["privacy.clearOnShutdown.offlineApps", false],
      ["privacy.sanitize.sanitizeOnShutdown", true],
      ["privacy.clearOnShutdown.history", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  document = gBrowser.contentDocument;
  deleteOnCloseBox = document.getElementById("deleteOnClose");

  ok(
    !deleteOnCloseBox.checked,
    "DeleteOnClose is set accordingly to the prefs, deselected"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // When private browsing mode autostart is selected, the deleteOnClose Box is selected always
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", false],
      ["privacy.clearOnShutdown.cache", false],
      ["privacy.clearOnShutdown.offlineApps", false],
      ["privacy.sanitize.sanitizeOnShutdown", false],
      ["browser.privatebrowsing.autostart", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  document = gBrowser.contentDocument;
  deleteOnCloseBox = document.getElementById("deleteOnClose");

  ok(
    deleteOnCloseBox.checked,
    "DeleteOnClose is set accordingly to the private Browsing autostart pref, selected"
  );

  // Reset history mode
  let historyMode = document.getElementById("historyMode");
  historyMode.value = "remember";
  historyMode.doCommand();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});
