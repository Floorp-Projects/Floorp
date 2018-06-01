/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests that disabling the cache for a tab works as it should when toolboxes
// are not toggled.
/* import-globals-from helper_disable_cache.js */
loadHelperScript("helper_disable_cache.js");

add_task(async function() {
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);

  // Ensure that the setting is cleared after the test.
  registerCleanupFunction(() => {
    info("Resetting devtools.cache.disabled to false.");
    Services.prefs.setBoolPref("devtools.cache.disabled", false);
  });

  // Initialise tabs: 1 and 2 with a toolbox, 3 and 4 without.
  for (const tab of tabs) {
    await initTab(tab, tab.startToolbox);
  }

  // Ensure cache is enabled for all tabs.
  await checkCacheStateForAllTabs([true, true, true, true]);

  // Check the checkbox in tab 0 and ensure cache is disabled for tabs 0 and 1.
  await setDisableCacheCheckboxChecked(tabs[0], true);
  await checkCacheStateForAllTabs([false, false, true, true]);

  await finishUp();
});
