/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  await pushPref("apz.scrollend-event.content.enabled", true);
  await pushPref("dom.element.invokers.enabled", true);

  const dbg = await initDebugger(
    "doc-event-breakpoints.html",
    "event-breakpoints.js"
  );
  await selectSource(dbg, "event-breakpoints.js");
  await waitForSelectedSource(dbg, "event-breakpoints.js");
  const eventBreakpointsSource = findSource(dbg, "event-breakpoints.js");

  // We want to set each breakpoint individually to test adding/removing breakpoints, see Bug 1748589.
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");

  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 12);

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
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 20);
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Timer", "timer.timeout.set");
  await toggleEventBreakpoint(dbg, "Timer", "timer.timeout.fire");
  invokeInTab("timerHandler");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 27);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 28);
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Script", "script.source.firstStatement");
  invokeInTab("evalHandler");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "eval-test.js").id, 2);
  await resume(dbg);

  await toggleEventBreakpoint(dbg, "Control", "event.control.focusin");
  await toggleEventBreakpoint(dbg, "Control", "event.control.focusout");
  invokeOnElement("#focus-text", "focus");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 43);
  await resume(dbg);

  // wait for focus-out event to fire
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 48);
  await resume(dbg);

  info("Deselect focus events");
  // We need to give the input focus to test composition, but we don't want the
  // focus breakpoints to fire.
  await toggleEventBreakpoint(dbg, "Control", "event.control.focusin");
  await toggleEventBreakpoint(dbg, "Control", "event.control.focusout");

  await toggleEventBreakpoint(dbg, "Control", "event.control.invoke");
  invokeOnElement("#invoker", "click");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 73);
  await resume(dbg);

  await toggleEventBreakpoint(
    dbg,
    "Keyboard",
    "event.keyboard.compositionstart"
  );
  invokeOnElement("#focus-text", "focus");

  info("Type some characters during composition");
  invokeComposition();

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 53);
  await resume(dbg);

  info("Deselect compositionstart and select compositionupdate");
  await toggleEventBreakpoint(
    dbg,
    "Keyboard",
    "event.keyboard.compositionstart"
  );
  await toggleEventBreakpoint(
    dbg,
    "Keyboard",
    "event.keyboard.compositionupdate"
  );

  invokeOnElement("#focus-text", "focus");

  info("Type some characters during composition");
  invokeComposition();

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 58);
  await resume(dbg);

  info("Deselect compositionupdate and select compositionend");
  await toggleEventBreakpoint(
    dbg,
    "Keyboard",
    "event.keyboard.compositionupdate"
  );
  await toggleEventBreakpoint(dbg, "Keyboard", "event.keyboard.compositionend");
  invokeOnElement("#focus-text", "focus");

  info("Type some characters during composition");
  invokeComposition();

  info("Commit the composition");
  EventUtils.synthesizeComposition({
    type: "compositioncommitasis",
    key: { key: "KEY_Enter" },
  });

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 63);
  await resume(dbg);

  info(`Check that breakpoint can be set on "scrollend"`);
  await toggleEventBreakpoint(dbg, "Control", "event.control.scrollend");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.scrollTo({ top: 20, behavior: "smooth" });
  });

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 68);
  await resume(dbg);

  info("Check that the click event breakpoint is still enabled");
  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 12);
  await resume(dbg);

  info("Check that disabling an event breakpoint works");
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");
  invokeInTab("clickHandler");
  // wait for a bit to make sure the debugger do not pause
  await wait(100);
  assertNotPaused(dbg);

  info("Check that we can re-enable event breakpoints");
  await toggleEventBreakpoint(dbg, "Mouse", "event.mouse.click");
  invokeInTab("clickHandler");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, eventBreakpointsSource.id, 12);
  await resume(dbg);

  info(
    "Test that we don't pause on event breakpoints when source is blackboxed."
  );
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX_WHOLE_SOURCES");

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
  await waitForDispatch(dbg.store, "UNBLACKBOX_WHOLE_SOURCES");
});

add_task(async function checkUnavailableEvents() {
  await pushPref("apz.scrollend-event.content.enabled", false);

  const dbg = await initDebugger(
    "doc-event-breakpoints.html",
    "event-breakpoints.js"
  );
  await selectSource(dbg, "event-breakpoints.js");
  await waitForSelectedSource(dbg, "event-breakpoints.js");

  is(
    await getEventBreakpointCheckbox(dbg, "Control", "event.control.scrollend"),
    null,
    `"scrollend" item is not displayed when "apz.scrollend-event.content.enabled" is false`
  );
});

function getEventListenersPanel(dbg) {
  return findElementWithSelector(dbg, ".event-listeners-pane .event-listeners");
}

async function toggleEventBreakpoint(
  dbg,
  eventBreakpointGroup,
  eventBreakpointName
) {
  const eventCheckbox = await getEventBreakpointCheckbox(
    dbg,
    eventBreakpointGroup,
    eventBreakpointName
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

async function getEventBreakpointCheckbox(
  dbg,
  eventBreakpointGroup,
  eventBreakpointName
) {
  if (!getEventListenersPanel(dbg)) {
    // Event listeners panel is collapsed, expand it
    findElementWithSelector(
      dbg,
      `.event-listeners-pane ._header .header-label`
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

  return findElementWithSelector(dbg, `input[value="${eventBreakpointName}"]`);
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

function invokeComposition() {
  const string = "ex";
  EventUtils.synthesizeCompositionChange({
    composition: {
      string,
      clauses: [
        {
          length: string.length,
          attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE,
        },
      ],
    },
    caret: { start: string.length, length: 0 },
    key: { key: string[string.length - 1] },
  });
}
