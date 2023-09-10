/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

add_setup(() => {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.alternate.enabled");
  });
});

add_task(async function browser_fixup_alternate_enabled() {
  Services.prefs.setBoolPref("browser.fixup.alternate.enabled", true);
  Services.prefs.setIntPref("browser.migration.version", 139);

  gBrowserGlue.observe(null, "browser-glue-test", "force-ui-migration");

  Assert.ok(
    !Services.prefs.getBoolPref("browser.fixup.alternate.enabled", false),
    "browser.fixup.alternate.enabled pref should be cleared"
  );
});
