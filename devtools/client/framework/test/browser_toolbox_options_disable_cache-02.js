/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests that disabling the cache for a tab works as it should when toolboxes
// are toggled.
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

  // Disable cache in tab 0
  await setDisableCacheCheckboxChecked(tabs[0], true);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = await gDevTools.showToolbox(tabs[2].target, "options");
  await checkCacheEnabled(tabs[2], false);

  // Close toolbox in tab 2 and ensure the cache is enabled again
  await tabs[2].toolbox.destroy();
  tabs[2].target = TargetFactory.forTab(tabs[2].tab);
  await checkCacheEnabled(tabs[2], true);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = await gDevTools.showToolbox(tabs[2].target, "options");
  await checkCacheEnabled(tabs[2], false);

  // Check the checkbox in tab 2 and ensure cache is enabled for all tabs.
  await setDisableCacheCheckboxChecked(tabs[2], false);
  await checkCacheStateForAllTabs([true, true, true, true]);

  await finishUp();
});
