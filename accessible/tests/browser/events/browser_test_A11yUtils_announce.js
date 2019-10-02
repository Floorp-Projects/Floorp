/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Check that the browser A11yUtils.announce() function works correctly.
// Note that this does not use mozilla::a11y::Accessible::Announce and a11y
// announcement events, as these aren't yet supported on desktop.
async function runTests() {
  const alert = document.getElementById("a11y-announcement");
  let alerted = waitForEvent(EVENT_ALERT, alert);
  A11yUtils.announce("first");
  let event = await alerted;
  const alertAcc = event.accessible;
  is(alertAcc.role, ROLE_ALERT);
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  is(alertAcc.firstChild.name, "first");

  alerted = waitForEvent(EVENT_ALERT, alertAcc);
  A11yUtils.announce("second");
  event = await alerted;
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  is(alertAcc.firstChild.name, "second");
}

addAccessibleTask(``, runTests);
