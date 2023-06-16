"use strict";

registerCleanupFunction(() => {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TRACKING_PREF);
});

add_setup(async function () {
  await UrlClassifierTestUtils.addTestTrackers();
});

add_task(async function test_shim_disabled_by_own_pref() {
  // Test that a shim will not apply if disabled in about:config

  Services.prefs.setBoolPref(DISABLE_SHIM1_PREF, true);
  Services.prefs.setBoolPref(TRACKING_PREF, true);

  await testShimDoesNotRun();

  Services.prefs.clearUserPref(DISABLE_SHIM1_PREF);
  Services.prefs.clearUserPref(TRACKING_PREF);
});

add_task(async function test_shim_disabled_by_global_pref() {
  // Test that a shim will not apply if disabled in about:config

  Services.prefs.setBoolPref(GLOBAL_PREF, false);
  Services.prefs.setBoolPref(DISABLE_SHIM1_PREF, false);
  Services.prefs.setBoolPref(TRACKING_PREF, true);

  await testShimDoesNotRun();

  Services.prefs.clearUserPref(GLOBAL_PREF);
  Services.prefs.clearUserPref(DISABLE_SHIM1_PREF);
  Services.prefs.clearUserPref(TRACKING_PREF);
});

add_task(async function test_shim_disabled_hosts_notHosts() {
  Services.prefs.setBoolPref(TRACKING_PREF, true);

  await testShimDoesNotRun(false, SHIMMABLE_TEST_PAGE_3);

  Services.prefs.clearUserPref(TRACKING_PREF);
});

add_task(async function test_shim_disabled_overridden_by_pref() {
  Services.prefs.setBoolPref(TRACKING_PREF, true);

  await testShimDoesNotRun(false, SHIMMABLE_TEST_PAGE_2);

  Services.prefs.setBoolPref(DISABLE_SHIM2_PREF, false);

  await testShimRuns(SHIMMABLE_TEST_PAGE_2);

  Services.prefs.clearUserPref(TRACKING_PREF);
  Services.prefs.clearUserPref(DISABLE_SHIM2_PREF);
});

add_task(async function test_shim() {
  // Test that a shim which only runs in strict mode works, and that it
  // is permitted to opt into showing normally-blocked tracking content.

  Services.prefs.setBoolPref(TRACKING_PREF, true);

  await testShimRuns(SHIMMABLE_TEST_PAGE);

  // test that if the user opts in on one domain, they will still have to opt
  // in on another domain which embeds an iframe to the first one.

  await testShimRuns(EMBEDDING_TEST_PAGE, 0, false, false);

  Services.prefs.clearUserPref(TRACKING_PREF);
});
