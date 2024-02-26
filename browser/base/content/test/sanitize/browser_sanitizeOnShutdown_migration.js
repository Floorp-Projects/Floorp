/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.useOldClearHistoryDialog", false]],
  });
});

add_task(async function testMigrationOfCacheAndSiteSettings() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cache", true],
      ["privacy.clearOnShutdown.siteSettings", true],
      ["privacy.clearOnShutdown_v2.cache", false],
      ["privacy.clearOnShutdown_v2.siteSettings", false],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.cache"),
    true,
    "Cache should be set to true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.siteSettings"),
    true,
    "siteSettings should be set to true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cache"),
    true,
    "old cache should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.siteSettings"),
    true,
    "old siteSettings should remain true"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function testHistoryAndFormData_historyTrue() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.history", true],
      ["privacy.clearOnShutdown.formdata", false],
      ["privacy.clearOnShutdown_v2.historyFormDataAndDownloads", false],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.clearOnShutdown_v2.historyFormDataAndDownloads"
    ),
    true,
    "historyFormDataAndDownloads should be set to true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.history"),
    true,
    "old history pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.formdata"),
    false,
    "old formdata pref should remain false"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function testHistoryAndFormData_historyFalse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.history", false],
      ["privacy.clearOnShutdown.formdata", true],
      ["privacy.clearOnShutdown_v2.historyFormDataAndDownloads", true],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.clearOnShutdown_v2.historyFormDataAndDownloads"
    ),
    false,
    "historyFormDataAndDownloads should be set to true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.history"),
    false,
    "old history pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.formdata"),
    true,
    "old formdata pref should remain true"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function testCookiesAndStorage_cookiesFalse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", false],
      ["privacy.clearOnShutdown.offlineApps", true],
      ["privacy.clearOnShutdown.sessions", true],
      ["privacy.clearOnShutdown_v2.cookiesAndStorage", true],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  // Simulate clearing on shutdown.
  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.cookiesAndStorage"),
    false,
    "cookiesAndStorage should be set to false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    false,
    "old cookies pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    true,
    "old offlineApps pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.sessions"),
    true,
    "old sessions pref should remain true"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function testCookiesAndStorage_cookiesTrue() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", true],
      ["privacy.clearOnShutdown.offlineApps", false],
      ["privacy.clearOnShutdown.sessions", false],
      ["privacy.clearOnShutdown_v2.cookiesAndStorage", false],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.cookiesAndStorage"),
    true,
    "cookiesAndStorage should be set to true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    true,
    "old cookies pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    false,
    "old offlineApps pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.sessions"),
    false,
    "old sessions pref should remain false"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function testMigrationDoesNotRepeat() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.cookies", true],
      ["privacy.clearOnShutdown.offlineApps", false],
      ["privacy.clearOnShutdown.sessions", false],
      ["privacy.clearOnShutdown_v2.cookiesAndStorage", false],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", true],
    ],
  });

  // Simulate clearing on shutdown.
  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.cookiesAndStorage"),
    false,
    "cookiesAndStorage should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    true,
    "old cookies pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    false,
    "old offlineApps pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.sessions"),
    false,
    "old sessions pref should remain false"
  );

  Assert.equal(
    Services.prefs.getBoolPref(
      "privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs"
    ),
    true,
    "migration pref has been flipped"
  );
});

add_task(async function ensureNoOldPrefsAreEffectedByMigration() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.clearOnShutdown.history", true],
      ["privacy.clearOnShutdown.formdata", true],
      ["privacy.clearOnShutdown.cookies", true],
      ["privacy.clearOnShutdown.offlineApps", false],
      ["privacy.clearOnShutdown.sessions", false],
      ["privacy.clearOnShutdown.siteSettings", true],
      ["privacy.clearOnShutdown.cache", true],
      ["privacy.clearOnShutdown_v2.cookiesAndStorage", false],
      ["privacy.sanitize.clearOnShutdown.hasMigratedToNewPrefs", false],
    ],
  });

  Sanitizer.runSanitizeOnShutdown();

  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown_v2.cookiesAndStorage"),
    true,
    "cookiesAndStorage should become true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies"),
    true,
    "old cookies pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps"),
    false,
    "old offlineApps pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.sessions"),
    false,
    "old sessions pref should remain false"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.history"),
    true,
    "old history pref should remain true"
  );
  Assert.equal(
    Services.prefs.getBoolPref("privacy.clearOnShutdown.formdata"),
    true,
    "old formdata pref should remain true"
  );
});
