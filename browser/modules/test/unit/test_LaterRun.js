"use strict";

const kEnabledPref = "browser.laterrun.enabled";
const kPagePrefRoot = "browser.laterrun.pages.";
const kSessionCountPref = "browser.laterrun.bookkeeping.sessionCount";
const kProfileCreationTime = "browser.laterrun.bookkeeping.profileCreationTime";

const { LaterRun } = ChromeUtils.import("resource:///modules/LaterRun.jsm");

Services.prefs.setBoolPref(kEnabledPref, true);
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo();

add_task(async function test_page_applies() {
  Services.prefs.setCharPref(
    kPagePrefRoot + "test_LaterRun_unittest.url",
    "https://www.mozilla.org/%VENDOR%/%NAME%/%ID%/%VERSION%/"
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumHoursSinceInstall",
    10
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumSessionCount",
    3
  );

  let pages = LaterRun.readPages();
  // We have to filter the pages because it's possible Firefox ships with other URLs
  // that get included in this test.
  pages = pages.filter(
    page => page.pref == kPagePrefRoot + "test_LaterRun_unittest."
  );
  Assert.equal(pages.length, 1, "Got 1 page");
  let page = pages[0];
  Assert.equal(
    page.pref,
    kPagePrefRoot + "test_LaterRun_unittest.",
    "Should know its own pref"
  );
  Assert.equal(
    page.minimumHoursSinceInstall,
    10,
    "Needs to have 10 hours since install"
  );
  Assert.equal(page.minimumSessionCount, 3, "Needs to have 3 sessions");
  Assert.equal(page.requireBoth, false, "Either requirement is enough");
  let expectedURL =
    "https://www.mozilla.org/" +
    Services.appinfo.vendor +
    "/" +
    Services.appinfo.name +
    "/" +
    Services.appinfo.ID +
    "/" +
    Services.appinfo.version +
    "/";
  Assert.equal(page.url, expectedURL, "URL is stored correctly");

  Assert.ok(
    page.applies({ hoursSinceInstall: 1, sessionCount: 3 }),
    "Applies when session count has been met."
  );
  Assert.ok(
    page.applies({ hoursSinceInstall: 1, sessionCount: 4 }),
    "Applies when session count has been exceeded."
  );
  Assert.ok(
    page.applies({ hoursSinceInstall: 10, sessionCount: 2 }),
    "Applies when total session time has been met."
  );
  Assert.ok(
    page.applies({ hoursSinceInstall: 20, sessionCount: 2 }),
    "Applies when total session time has been exceeded."
  );
  Assert.ok(
    page.applies({ hoursSinceInstall: 10, sessionCount: 3 }),
    "Applies when both time and session count have been met."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 1 }),
    "Does not apply when neither time and session count have been met."
  );

  page.requireBoth = true;

  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 3 }),
    "Does not apply when only session count has been met."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 4 }),
    "Does not apply when only session count has been exceeded."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 10, sessionCount: 2 }),
    "Does not apply when only total session time has been met."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 20, sessionCount: 2 }),
    "Does not apply when only total session time has been exceeded."
  );
  Assert.ok(
    page.applies({ hoursSinceInstall: 10, sessionCount: 3 }),
    "Applies when both time and session count have been met."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 1 }),
    "Does not apply when neither time and session count have been met."
  );

  // Check that pages that have run never apply:
  Services.prefs.setBoolPref(
    kPagePrefRoot + "test_LaterRun_unittest.hasRun",
    true
  );
  page.requireBoth = false;

  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 3 }),
    "Does not apply when page has already run (sessionCount equal)."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 4 }),
    "Does not apply when page has already run (sessionCount exceeding)."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 10, sessionCount: 2 }),
    "Does not apply when page has already run (hoursSinceInstall equal)."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 20, sessionCount: 2 }),
    "Does not apply when page has already run (hoursSinceInstall exceeding)."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 10, sessionCount: 3 }),
    "Does not apply when page has already run (both criteria equal)."
  );
  Assert.ok(
    !page.applies({ hoursSinceInstall: 1, sessionCount: 1 }),
    "Does not apply when page has already run (both criteria insufficient anyway)."
  );

  clearAllPagePrefs();
});

add_task(async function test_get_URL() {
  Services.prefs.setIntPref(
    kProfileCreationTime,
    Math.floor((Date.now() - 11 * 60 * 60 * 1000) / 1000)
  );
  Services.prefs.setCharPref(
    kPagePrefRoot + "test_LaterRun_unittest.url",
    "https://www.mozilla.org/"
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumHoursSinceInstall",
    10
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumSessionCount",
    3
  );
  let pages = LaterRun.readPages();
  // We have to filter the pages because it's possible Firefox ships with other URLs
  // that get included in this test.
  pages = pages.filter(
    page => page.pref == kPagePrefRoot + "test_LaterRun_unittest."
  );
  Assert.equal(pages.length, 1, "Should only be 1 matching page");
  let page = pages[0];
  let url;
  do {
    url = LaterRun.getURL();
    // We have to loop because it's possible Firefox ships with other URLs that get triggered by
    // this test.
  } while (url && url != "https://www.mozilla.org/");
  Assert.equal(
    url,
    "https://www.mozilla.org/",
    "URL should be as expected when prefs are set."
  );
  Assert.ok(
    Services.prefs.prefHasUserValue(
      kPagePrefRoot + "test_LaterRun_unittest.hasRun"
    ),
    "Should have set pref"
  );
  Assert.ok(
    Services.prefs.getBoolPref(kPagePrefRoot + "test_LaterRun_unittest.hasRun"),
    "Should have set pref to true"
  );
  Assert.ok(page.hasRun, "Other page objects should know it has run, too.");

  clearAllPagePrefs();
});

add_task(async function test_insecure_urls() {
  Services.prefs.setCharPref(
    kPagePrefRoot + "test_LaterRun_unittest.url",
    "http://www.mozilla.org/"
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumHoursSinceInstall",
    10
  );
  Services.prefs.setIntPref(
    kPagePrefRoot + "test_LaterRun_unittest.minimumSessionCount",
    3
  );
  let pages = LaterRun.readPages();
  // We have to filter the pages because it's possible Firefox ships with other URLs
  // that get triggered in this test.
  pages = pages.filter(
    page => page.pref == kPagePrefRoot + "test_LaterRun_unittest."
  );
  Assert.equal(pages.length, 0, "URL with non-https scheme should get ignored");
  clearAllPagePrefs();
});

add_task(async function test_dynamic_pref_getter_setter() {
  delete LaterRun._sessionCount;
  Services.prefs.setIntPref(kSessionCountPref, 0);
  Assert.equal(LaterRun.sessionCount, 0, "Should start at 0");

  LaterRun.sessionCount++;
  Assert.equal(LaterRun.sessionCount, 1, "Should increment.");
  Assert.equal(
    Services.prefs.getIntPref(kSessionCountPref),
    1,
    "Should update pref"
  );
});

function clearAllPagePrefs() {
  let allChangedPrefs = Services.prefs.getChildList(kPagePrefRoot);
  for (let pref of allChangedPrefs) {
    Services.prefs.clearUserPref(pref);
  }
}
