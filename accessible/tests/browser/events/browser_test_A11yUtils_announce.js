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
  A11yUtils.announce({ raw: "first" });
  let event = await alerted;
  const alertAcc = event.accessible;
  is(alertAcc.role, ROLE_ALERT);
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  is(alertAcc.firstChild.name, "first");

  alerted = waitForEvent(EVENT_ALERT, alertAcc);
  A11yUtils.announce({ raw: "second" });
  event = await alerted;
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  is(alertAcc.firstChild.name, "second");

  info("Testing Fluent message");
  // We need a simple Fluent message here without arguments or attributes.
  const fluentId = "search-one-offs-with-title";
  const fluentMessage = await document.l10n.formatValue(fluentId);
  alerted = waitForEvent(EVENT_ALERT, alertAcc);
  A11yUtils.announce({ id: fluentId });
  event = await alerted;
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  is(alertAcc.firstChild.name, fluentMessage);

  info("Ensuring Fluent message is cancelled if announce is re-entered");
  alerted = waitForEvent(EVENT_ALERT, alertAcc);
  // This call runs async.
  let asyncAnnounce = A11yUtils.announce({ id: fluentId });
  // Before the async call finishes, call announce again.
  A11yUtils.announce({ raw: "third" });
  // Wait for the async call to complete.
  await asyncAnnounce;
  event = await alerted;
  ok(!alertAcc.name);
  is(alertAcc.childCount, 1);
  // The async call should have been cancelled. If it wasn't, we would get
  // fluentMessage here instead of "third".
  is(alertAcc.firstChild.name, "third");
}

addAccessibleTask(``, runTests);
