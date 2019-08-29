/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Tests that click events that close the current event tooltip are still propagated to
// the target underneath.

const TEST_URL = `
  <body>
    <div id="d1" onclick="console.log(1)">test</div>
    <!--                                        -->
    <!-- adding some comments to make sure      -->
    <!-- the second event icon is not hidden by -->
    <!-- the tooltip of the first event icon    -->
    <!--                                        -->
    <div id="d2" onclick="console.log(2)">test</div>
  </body>
`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL)
  );

  await inspector.markup.expandAll();

  const container1 = await getContainerForSelector("#d1", inspector);
  const evHolder1 = container1.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );

  const container2 = await getContainerForSelector("#d2", inspector);
  const evHolder2 = container2.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );

  const tooltip = inspector.markup.eventDetailsTooltip;

  info("Click the event icon for the first element");
  let onShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder1, {}, inspector.markup.win);
  await onShown;
  info("event tooltip for the first div is shown");

  info("Click the event icon for the second element");
  let onHidden = tooltip.once("hidden");
  onShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder2, {}, inspector.markup.win);

  await onHidden;
  info("previous tooltip hidden");

  await onShown;
  info("event tooltip for the second div is shown");

  info("Check that clicking on evHolder2 again hides the tooltip");
  onHidden = tooltip.once("hidden");
  EventUtils.synthesizeMouseAtCenter(evHolder2, {}, inspector.markup.win);
  await onHidden;

  info("Check that the tooltip does not reappear immediately after");
  await waitForTime(1000);
  is(
    tooltip.isVisible(),
    false,
    "The tooltip is still hidden after waiting for one second"
  );

  info("Open the tooltip on evHolder2 again");
  onShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder2, {}, inspector.markup.win);
  await onShown;

  info("Click on the computed view tab");
  const onHighlighterHidden = inspector.highlighter.once("node-unhighlight");
  const onTabComputedViewSelected = inspector.sidebar.once(
    "computedview-selected"
  );
  const computedViewTab = inspector.panelDoc.querySelector("#computedview-tab");
  EventUtils.synthesizeMouseAtCenter(
    computedViewTab,
    {},
    inspector.panelDoc.defaultView
  );

  await onTabComputedViewSelected;
  info("computed view was selected");

  await onHighlighterHidden;
  info(
    "box model highlighter hidden after moving the mouse out of the markup view"
  );
});
