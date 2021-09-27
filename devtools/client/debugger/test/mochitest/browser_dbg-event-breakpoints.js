/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
    "event.control.focusin",
    "event.control.focusout",
    "event.mouse.click",
    "event.xhr.load",
    "timer.timeout.set",
    "timer.timeout.fire",
    "script.source.firstStatement",
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

  invokeInTab("evalHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 2, "http://example.com/eval-test.js");
  await resume(dbg);

  invokeOnElement("#focus-text", "focus");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 43);
  await resume(dbg);

  // wait for focus-out event to fire
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 48);
  await resume(dbg);

  // Test that we don't pause on event breakpoints when source is blackboxed.
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");

  invokeInTab("clickHandler");
  is(isPaused(dbg), false);

  invokeInTab("xhrHandler");
  is(isPaused(dbg), false);

  invokeInTab("timerHandler");
  is(isPaused(dbg), false);

  // Cleanup - unblackbox the source
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");
});

function assertPauseLocation(dbg, line, url = "event-breakpoints.js") {
  const { location } = dbg.selectors.getVisibleSelectedFrame();

  const source = findSource(dbg, url);

  is(location.sourceId, source.id, `correct sourceId`);
  is(location.line, line, `correct line`);

  assertPausedLocation(dbg);
}

async function invokeOnElement(selector, action) {
  await SpecialPowers.focus(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [selector, action], (_selector, _action) => {
    content.document.querySelector(_selector)[_action]();
  });
}
