/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that event listeners can be disabled and re-enabled from the markup view event bubble.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_toggle.html";

loadHelperScript("helper_events_test_runner.js");

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);
  const { resourceCommand } = toolbox.commands;
  await inspector.markup.expandAll();
  await selectNode("#target", inspector);

  info(
    "Click on the target element to make sure the event listeners are properly set"
  );
  // There's a "mouseup" event listener that is `console.info` (so we can check "native" events).
  // In order to know if it was called, we listen for the next console.info resource.
  let { onResource: onConsoleInfoMessage } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.message.level == "info";
        },
      }
    );
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");

  let data = await getTargetElementHandledEventData();
  is(data.click, 1, `target handled one "click" event`);
  is(data.mousedown, 1, `target handled one "mousedown" event`);
  await onConsoleInfoMessage;
  ok(true, `the "mouseup" event listener (console.info) was called`);

  info("Check that the event tooltip has the expected content");
  const container = await getContainerForSelector("#target", inspector);
  const eventTooltipBadge = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );
  ok(eventTooltipBadge, "The event tooltip badge is displayed");

  const tooltip = inspector.markup.eventDetailsTooltip;
  let onTooltipShown = tooltip.once("shown");
  eventTooltipBadge.click();
  await onTooltipShown;
  ok(true, "The tooltip is shown");

  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click [x]", "mousedown [x]", "mouseup [x]"],
    "The expected events are displayed, all enabled"
  );
  ok(
    !eventTooltipBadge.classList.contains("has-disabled-events"),
    "The event badge does not have the has-disabled-events class"
  );

  const [clickHeader, mousedownHeader, mouseupHeader] =
    getHeadersInEventTooltip(tooltip);

  info("Uncheck the mousedown event checkbox");
  await toggleEventListenerCheckbox(tooltip, mousedownHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click [x]", "mousedown []", "mouseup [x]"],
    "mousedown checkbox was unchecked"
  );
  ok(
    eventTooltipBadge.classList.contains("has-disabled-events"),
    "Unchecking an event applied the has-disabled-events class to the badge"
  );
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  data = await getTargetElementHandledEventData();
  is(data.click, 2, `target handled another "click" event…`);
  is(data.mousedown, 1, `… but not a mousedown one`);

  info(
    "Check that the event badge style is reset when re-enabling all disabled events"
  );
  await toggleEventListenerCheckbox(tooltip, mousedownHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click [x]", "mousedown [x]", "mouseup [x]"],
    "mousedown checkbox is checked again"
  );
  ok(
    !eventTooltipBadge.classList.contains("has-disabled-events"),
    "The event badge does not have the has-disabled-events class after re-enabling disabled event"
  );
  info("Disable mousedown again for the rest of the test");
  await toggleEventListenerCheckbox(tooltip, mousedownHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click [x]", "mousedown []", "mouseup [x]"],
    "mousedown checkbox is unchecked again"
  );

  info("Uncheck the click event checkbox");
  await toggleEventListenerCheckbox(tooltip, clickHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click []", "mousedown []", "mouseup [x]"],
    "click checkbox was unchecked"
  );
  ok(
    eventTooltipBadge.classList.contains("has-disabled-events"),
    "event badge still has the has-disabled-events class"
  );
  ({ onResource: onConsoleInfoMessage } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.message.level == "info";
        },
      }
    ));
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  data = await getTargetElementHandledEventData();
  is(data.click, 2, `click event listener was disabled`);
  is(data.mousedown, 1, `and mousedown still is disabled as well`);
  await onConsoleInfoMessage;
  ok(true, `the "click" event listener (console.info) was called`);

  info("Uncheck the mouseup event checkbox");
  await toggleEventListenerCheckbox(tooltip, mouseupHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click []", "mousedown []", "mouseup []"],
    "mouseup checkbox was unchecked"
  );

  ({ onResource: onConsoleInfoMessage } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.message.level == "info";
        },
      }
    ));
  const onTimeout = wait(500).then(() => "TIMEOUT");
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  const raceResult = await Promise.race([onConsoleInfoMessage, onTimeout]);
  is(
    raceResult,
    "TIMEOUT",
    "The mouseup event didn't trigger a console.info call, meaning the event listener was disabled"
  );

  info("Re-enable the mousedown event");
  await toggleEventListenerCheckbox(tooltip, mousedownHeader);
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click []", "mousedown [x]", "mouseup []"],
    "mousedown checkbox is checked again"
  );
  ok(
    eventTooltipBadge.classList.contains("has-disabled-events"),
    "event badge still has the has-disabled-events class"
  );
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  data = await getTargetElementHandledEventData();
  is(data.click, 2, `no additional "click" event were handled`);
  is(
    data.mousedown,
    2,
    `but we did get a new "mousedown", the event listener was re-enabled`
  );

  info("Hide the tooltip and show it again");
  const tooltipHidden = tooltip.once("hidden");
  tooltip.hide();
  await tooltipHidden;

  onTooltipShown = tooltip.once("shown");
  eventTooltipBadge.click();
  await onTooltipShown;
  ok(true, "The tooltip is shown again");

  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click []", "mousedown [x]", "mouseup []"],
    "Only mousedown checkbox is checked"
  );

  info("Re-enable mouseup events");
  await toggleEventListenerCheckbox(
    tooltip,
    getHeadersInEventTooltip(tooltip).at(-1)
  );
  Assert.deepEqual(
    getAsciiHeadersViz(tooltip),
    ["click []", "mousedown [x]", "mouseup [x]"],
    "mouseup is checked again"
  );

  ({ onResource: onConsoleInfoMessage } =
    await resourceCommand.waitForNextResource(
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.message.level == "info";
        },
      }
    ));
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  await onConsoleInfoMessage;
  ok(true, "The mouseup event was re-enabled");
  data = await getTargetElementHandledEventData();
  is(data.click, 2, `"click" is still disabled`);
  is(
    data.mousedown,
    3,
    `we received a new "mousedown" event as part of the click`
  );

  info("Close DevTools to check that event listeners are re-enabled");
  await closeToolbox();
  await safeSynthesizeMouseEventAtCenterInContentPage("#target");
  data = await getTargetElementHandledEventData();
  is(
    data.click,
    3,
    `a new "click" event was handled after the devtools was closed`
  );
  is(
    data.mousedown,
    4,
    `a new "mousedown" event was handled after the devtools was closed`
  );
});

function getHeadersInEventTooltip(tooltip) {
  return Array.from(tooltip.panel.querySelectorAll(".event-header"));
}

/**
 * Get an array of string representing a header in its state, e.g.
 * [
 *   "click [x]",
 *   "mousedown []",
 * ]
 *
 * represents an event tooltip with a click and a mousedown event, where the mousedown
 * event has been disabled.
 *
 * @param {EventTooltip} tooltip
 * @returns Array<String>
 */
function getAsciiHeadersViz(tooltip) {
  return getHeadersInEventTooltip(tooltip).map(
    el =>
      `${el.querySelector(".event-tooltip-event-type").textContent} [${
        getHeaderCheckbox(el).checked ? "x" : ""
      }]`
  );
}

function getHeaderCheckbox(headerEl) {
  return headerEl.querySelector("input[type=checkbox]");
}

async function toggleEventListenerCheckbox(tooltip, headerEl) {
  const onEventToggled = tooltip.eventTooltip.once(
    "event-tooltip-listener-toggled"
  );
  const checkbox = getHeaderCheckbox(headerEl);
  const previousValue = checkbox.checked;
  EventUtils.synthesizeMouseAtCenter(
    getHeaderCheckbox(headerEl),
    {},
    headerEl.ownerGlobal
  );
  await onEventToggled;
  is(checkbox.checked, !previousValue, "The checkbox was toggled");
  is(
    headerEl.classList.contains("content-expanded"),
    false,
    "Clicking on the checkbox did not expand the header"
  );
}

/**
 * @returns Promise<Object> The object keys are event names (e.g. "click", "mousedown"), and
 *          the values are number representing the number of time the event was handled.
 *          Note that "mouseup" isn't handled here.
 */
function getTargetElementHandledEventData() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    // In doc_markup_events_toggle.html , we count the events handled by the target in
    // a stringified object in dataset.handledEvents.
    return JSON.parse(
      content.document.getElementById("target").dataset.handledEvents
    );
  });
}
