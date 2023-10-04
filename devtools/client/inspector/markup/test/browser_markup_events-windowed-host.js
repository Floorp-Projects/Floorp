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

add_task(async function () {
  info(
    "Switch to 2 pane inspector to avoid sidebar width issues with opening events"
  );
  await pushPref("devtools.inspector.three-pane-enabled", false);
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);
  await runTests(inspector);

  await toolbox.switchHost("window");

  // Switching hosts is not correctly waiting when DevTools run in content frame
  // See Bug 1571421.
  await wait(1000);

  await runTests(inspector);

  await toolbox.switchHost("bottom");

  // Switching hosts is not correctly waiting when DevTools run in content frame
  // See Bug 1571421.
  await wait(1000);

  await runTests(inspector);

  await toolbox.destroy();
});

async function runTests(inspector) {
  const markupContainer = await getContainerForSelector("#events", inspector);
  const evHolder = markupContainer.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );
  const tooltip = inspector.markup.eventDetailsTooltip;

  info("Clicking to open event tooltip.");
  const onTooltipShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(
    evHolder,
    {},
    inspector.markup.doc.defaultView
  );
  await onTooltipShown;
  ok(tooltip.isVisible(), "EventTooltip visible.");

  info("Click on another tag to hide the event tooltip");
  const onTooltipHidden = tooltip.once("hidden");
  const script = await getContainerForSelector("script", inspector);
  const tag = script.elt.querySelector(".tag");
  EventUtils.synthesizeMouseAtCenter(tag, {}, inspector.markup.doc.defaultView);

  await onTooltipHidden;

  ok(!tooltip.isVisible(), "EventTooltip hidden.");
}
