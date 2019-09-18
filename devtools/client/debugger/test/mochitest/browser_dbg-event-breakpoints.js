/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function assertPauseLocation(dbg, line) {
  const { location } = dbg.selectors.getVisibleSelectedFrame();

  const source = findSource(dbg, "event-breakpoints.js");

  is(location.sourceId, source.id, `correct sourceId`);
  is(location.line, line, `correct line`);

  assertPausedLocation(dbg);
}

add_task(async function() {
  await pushPref(
    "devtools.debugger.features.event-listeners-breakpoints",
    true
  );

  const dbg = await initDebugger(
    "doc-event-breakpoints.html",
    "event-breakpoints"
  );
  await selectSource(dbg, "event-breakpoints");
  await waitForSelectedSource(dbg, "event-breakpoints");

  await dbg.actions.addEventListenerBreakpoints([
    "event.mouse.click",
    "event.xhr.load",
    "timer.timeout.set",
    "timer.timeout.fire",
  ]);

  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 12);
  await resume(dbg);

  invokeInTab("xhrHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 20);
  await resume(dbg);

  invokeInTab("timerHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 27);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPauseLocation(dbg, 28);
  await resume(dbg);
});
