/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that we can log event listeners calls
 */

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger(
    "doc-event-breakpoints.html",
    "event-breakpoints.js"
  );

  await clickElement(dbg, "logEventsCheckbox");
  await dbg.actions.addEventListenerBreakpoints(["event.mouse.click"]);
  clickElementInTab("#click-target");

  await hasConsoleMessage(dbg, "click");
  await waitForRequestsToSettle(dbg);
  ok(true);
});
