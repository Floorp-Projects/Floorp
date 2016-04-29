"use strict";

// make sure that the stub is present and enabled
add_task(function* stub_enabled() {
  let addon = yield new Promise(
    resolve => AddonManager.getAddonByID("webcompat@mozilla.org", resolve)
  );
  isnot(addon, null, "Webcompat stub addon should exist");
  is(addon.version, "1.0");
  is(addon.name, "Web Compat");
  ok(addon.isCompatible, "Webcompat stub addon is compatible with Firefox");
  ok(!addon.appDisabled, "Webcompat stub addon is not app disabled");
  ok(addon.isActive, "Webcompat stub addon is active");
  is(addon.type, "extension", "Webcompat stub addon is type extension");
});
