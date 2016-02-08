"use strict";

add_task(function*() {
  let pocketAddon = yield new Promise(resolve => {
    AddonManager.getAddonByID("firefox@getpocket.com", resolve);
  });
  if (!pocketAddon) {
    ok(true, "Pocket is not installed");
    return;
  }
  if (!Services.prefs.getBoolPref("extensions.pocket.enabled")) {
    ok(true, "Pocket add-on is not enabled");
    return;
  }

  for (let id of ["panelMenu_pocket", "menu_pocket", "BMB_pocket",
                  "panelMenu_pocketSeparator", "menu_pocketSeparator",
                  "BMB_pocketSeparator"]) {
    ok(document.getElementById(id), "Should see element with id " + id);
  }
});
