/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

add_task(async function testForceDisable() {
  ok(
    !Services.appinfo.accessibilityEnabled,
    "Accessibility is disabled by default"
  );

  info("Reset force disabled preference");
  Services.prefs.clearUserPref(PREF_ACCESSIBILITY_FORCE_DISABLED);

  info("Enable accessibility service via XPCOM");
  let a11yInit = initPromise();
  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await a11yInit;
  ok(Services.appinfo.accessibilityEnabled, "Accessibility is enabled");

  info("Force disable a11y service via preference");
  let a11yShutdown = shutdownPromise();
  Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
  await a11yShutdown;
  ok(!Services.appinfo.accessibilityEnabled, "Accessibility is disabled");

  info("Attempt to get an instance of a11y service and call its method.");
  accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  try {
    accService.getAccesssibleFor(document);
    ok(false, "getAccesssibleFor should've triggered an exception.");
  } catch (e) {
    ok(
      true,
      "getAccesssibleFor triggers an exception as a11y service is shutdown."
    );
  }
  ok(!Services.appinfo.accessibilityEnabled, "Accessibility is disabled");

  info("Reset force disabled preference");
  Services.prefs.clearUserPref(PREF_ACCESSIBILITY_FORCE_DISABLED);

  info("Create a11y service again");
  a11yInit = initPromise();
  accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await a11yInit;
  ok(Services.appinfo.accessibilityEnabled, "Accessibility is enabled");

  info("Remove all references to a11y service");
  a11yShutdown = shutdownPromise();
  accService = null;
  forceGC();
  await a11yShutdown;
  ok(!Services.appinfo.accessibilityEnabled, "Accessibility is disabled");
});
