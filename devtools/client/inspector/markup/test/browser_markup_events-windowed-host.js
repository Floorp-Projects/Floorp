/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/*
 * Test that the event details tooltip can be hidden by clicking outside of the tooltip
 * after switching hosts.
 */

const TEST_URL = URL_ROOT + "doc_markup_events-overflow.html";

registerCleanupFunction(() => {
  // Restore the default Toolbox host position after the test.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});

add_task(async function() {
  info("Switch to 2 pane inspector to avoid sidebar width issues with opening events");
  await pushPref("devtools.inspector.three-pane-enabled", false);
  let { inspector, toolbox } = await openInspectorForURL(TEST_URL);
  await runTests(inspector);

  await toolbox.switchHost("window");
  await runTests(inspector);

  await toolbox.switchHost("bottom");
  await runTests(inspector);

  await toolbox.destroy();
});

async function runTests(inspector) {
  let markupContainer = await getContainerForSelector("#events", inspector);
  let evHolder = markupContainer.elt.querySelector(".markupview-event-badge");
  let tooltip = inspector.markup.eventDetailsTooltip;

  info("Clicking to open event tooltip.");

  let onInspectorUpdated = inspector.once("inspector-updated");
  let onTooltipShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder, {}, inspector.markup.doc.defaultView);

  await onTooltipShown;
  // New node is selected when clicking on the events bubble, wait for inspector-updated.
  await onInspectorUpdated;

  ok(tooltip.isVisible(), "EventTooltip visible.");

  onInspectorUpdated = inspector.once("inspector-updated");
  let onTooltipHidden = tooltip.once("hidden");

  info("Click on another tag to hide the event tooltip");
  let h1 = await getContainerForSelector("h1", inspector);
  let tag = h1.elt.querySelector(".tag");
  EventUtils.synthesizeMouseAtCenter(tag, {}, inspector.markup.doc.defaultView);

  await onTooltipHidden;
  // New node is selected, wait for inspector-updated.
  await onInspectorUpdated;

  ok(!tooltip.isVisible(), "EventTooltip hidden.");
}
