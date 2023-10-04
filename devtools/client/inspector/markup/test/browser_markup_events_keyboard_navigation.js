/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that the event listeners popup can be used from the keyboard.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_toggle.html";

loadHelperScript("helper_events_test_runner.js");

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await inspector.markup.expandAll();
  await selectNode("#target", inspector);

  info("Check that the event tooltip has the expected content");
  const container = await getContainerForSelector("#target", inspector);
  const eventTooltipBadge = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );
  ok(eventTooltipBadge, "The event tooltip badge is displayed");

  const tooltip = inspector.markup.eventDetailsTooltip;
  const onTooltipShown = tooltip.once("shown");
  eventTooltipBadge.focus();
  EventUtils.synthesizeKey("VK_RETURN", {}, eventTooltipBadge.ownerGlobal);
  await onTooltipShown;
  ok(true, "The tooltip is shown");

  // TODO: More keyboard interactions will be added as part of Bug 1843330
});
