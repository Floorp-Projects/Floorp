"use strict";

add_task(function* test_enabled() {
  let addon = yield new Promise(
    resolve => AddonManager.getAddonByID("formautofill@mozilla.org", resolve)
  );
  isnot(addon, null, "Check addon exists");
  is(addon.version, "1.0", "Check version");
  is(addon.name, "Form Autofill", "Check name");
  ok(addon.isCompatible, "Check application compatibility");
  ok(!addon.appDisabled, "Check not app disabled");
  ok(addon.isActive, "Check addon is active");
  is(addon.type, "extension", "Check type is 'extension'");
});
