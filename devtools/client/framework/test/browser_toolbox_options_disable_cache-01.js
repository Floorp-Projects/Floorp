/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

// Tests that disabling the cache for a tab works as it should when toolboxes
// are not toggled.
loadHelperScript("helper_disable_cache.js");

add_task(function*() {
  // Ensure that the setting is cleared after the test.
  registerCleanupFunction(() => {
    info("Resetting devtools.cache.disabled to false.");
    Services.prefs.setBoolPref("devtools.cache.disabled", false);
  });

  // Initialise tabs: 1 and 2 with a toolbox, 3 and 4 without.
  for (let tab of tabs) {
    yield initTab(tab, tab.startToolbox);
  }

  // Ensure cache is enabled for all tabs.
  yield checkCacheStateForAllTabs([true, true, true, true]);

  // Check the checkbox in tab 0 and ensure cache is disabled for tabs 0 and 1.
  yield setDisableCacheCheckboxChecked(tabs[0], true);
  yield checkCacheStateForAllTabs([false, false, true, true]);

  yield finishUp();
});
