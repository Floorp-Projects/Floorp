/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Tests that disabling the cache for a tab works as it should when toolboxes
// are toggled.
loadHelperScript("helper_disable_cache.js");

add_task(function* () {
  // Ensure that the setting is cleared after the test.
  registerCleanupFunction(() => {
    info("Resetting devtools.cache.disabled to false.");
    Services.prefs.setBoolPref("devtools.cache.disabled", false);
  });

  // Initialise tabs: 1 and 2 with a toolbox, 3 and 4 without.
  for (let tab of tabs) {
    yield initTab(tab, tab.startToolbox);
  }

  // Disable cache in tab 0
  yield setDisableCacheCheckboxChecked(tabs[0], true);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = yield gDevTools.showToolbox(tabs[2].target, "options");
  yield checkCacheEnabled(tabs[2], false);

  // Close toolbox in tab 2 and ensure the cache is enabled again
  yield tabs[2].toolbox.destroy();
  tabs[2].target = TargetFactory.forTab(tabs[2].tab);
  yield checkCacheEnabled(tabs[2], true);

  // Open toolbox in tab 2 and ensure the cache is then disabled.
  tabs[2].toolbox = yield gDevTools.showToolbox(tabs[2].target, "options");
  yield checkCacheEnabled(tabs[2], false);

  // Check the checkbox in tab 2 and ensure cache is enabled for all tabs.
  yield setDisableCacheCheckboxChecked(tabs[2], false);
  yield checkCacheStateForAllTabs([true, true, true, true]);

  yield finishUp();
});
