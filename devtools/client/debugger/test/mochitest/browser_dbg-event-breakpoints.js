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

  // We want to set each breakpoint individually to test adding/removing breakpoints, see Bug 1748589.
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");

  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 12);

  const whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(
    whyPaused,
    `Paused on event breakpoint\nDOM 'click' event`,
    "whyPaused does state that the debugger is paused as a result of a click event breakpoint"
  );
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "XHR", "event.xhr.load");
  invokeInTab("xhrHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 20);
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Timer", "timer.timeout.set");
  await toggleEventBreakpoint(dbg, "Timer", "timer.timeout.fire");
  invokeInTab("timerHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 27);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPauseLocation(dbg, 28);
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Script", "script.source.firstStatement");
  invokeInTab("evalHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 2, "https://example.com/eval-test.js");
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Control", "event.control.focusin");
  await toggleEventBreakpoint(dbg, "Control", "event.control.focusout");
  invokeOnElement("#focus-text", "focus");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 43);
  await resume(dbg);

  // wait for focus-out event to fire
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 48);
  await resume(dbg);

  info("Check that the click event breakpoint is still enabled");
  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 12);
  await resume(dbg);

  info("Check that disabling an event breakpoint works");
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");
  invokeInTab("clickHandler");
  // wait for a bit to make sure the debugger do not pause
  await wait(100);
  assertNotPaused(dbg);

  info("Check that we can re-eanble event breakpoints");
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");
  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPauseLocation(dbg, 12);
  await resume(dbg);

  info(
    "Test that we don't pause on event breakpoints when source is blackboxed."
  );
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");

  invokeInTab("clickHandler");
  // wait for a bit to make sure the debugger do not pause
  await wait(100);
  assertNotPaused(dbg);

  invokeInTab("xhrHandler");
  // wait for a bit to make sure the debugger do not pause
  await wait(100);
  assertNotPaused(dbg);

  invokeInTab("timerHandler");
  // wait for a bit to make sure the debugger do not pause
  await wait(100);
  assertNotPaused(dbg);

  // Cleanup - unblackbox the source
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");
});

function getEventListenersPanel(dbg) {
  return findElementWithSelector(dbg, ".event-listeners-pane .event-listeners");
}

async function toggleEventBreakpoint(
  dbg,
  eventBreakpointGroup,
  eventBreakpointName
) {
  if (!getEventListenersPanel(dbg)) {
    // Event listeners panel is collapse, expand it
    findElementWithSelector(
      dbg,
      `.event-listeners-pane ._header .arrow`
    ).click();
    await waitFor(() => getEventListenersPanel(dbg));
  }

  const groupCheckbox = findElementWithSelector(
    dbg,
    `input[value="${eventBreakpointGroup}"]`
  );
  const groupEl = groupCheckbox.closest(".event-listener-group");
  let groupEventsUl = groupEl.querySelector("ul");
  if (!groupEventsUl) {
    info(
      `Expand ${eventBreakpointGroup} and wait for the sub list to be displayed`
    );
    groupEl.querySelector(".event-listener-expand").click();
    groupEventsUl = await waitFor(() => groupEl.querySelector("ul"));
  }

  const eventCheckbox = findElementWithSelector(
    dbg,
    `input[value="${eventBreakpointName}"]`
  );
  eventCheckbox.scrollIntoView();
  info(`Toggle ${eventBreakpointName} breakpoint`);
  const onEventListenersUpdate = waitForDispatch(
    dbg.store,
    "UPDATE_EVENT_LISTENERS"
  );
  eventCheckbox.click();
  await onEventListenersUpdate;
}

function assertPauseLocation(dbg, line, url = "event-breakpoints.js") {
  const { location } = dbg.selectors.getVisibleSelectedFrame();

  const source = findSource(dbg, url);

  is(location.sourceId, source.id, `correct sourceId`);
  is(location.line, line, `correct line`);

  assertPausedLocation(dbg);
}

async function invokeOnElement(selector, action) {
  await SpecialPowers.focus(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, action],
    (_selector, _action) => {
      content.document.querySelector(_selector)[_action]();
    }
  );
}
