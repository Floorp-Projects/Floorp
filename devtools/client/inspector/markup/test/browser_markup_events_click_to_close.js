/* vim: set ts=2 et sw=2 tw=80: */
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
  let {inspector, toolbox} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL));

  await inspector.markup.expandAll();

  let container1 = await getContainerForSelector("#d1", inspector);
  let evHolder1 = container1.elt.querySelector(".markupview-event-badge");

  let container2 = await getContainerForSelector("#d2", inspector);
  let evHolder2 = container2.elt.querySelector(".markupview-event-badge");

  let tooltip = inspector.markup.eventDetailsTooltip;

  info("Click the event icon for the first element");
  let onShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder1, {},
    inspector.markup.doc.defaultView);
  await onShown;
  info("event tooltip for the first div is shown");

  info("Click the event icon for the second element");
  let onHidden = tooltip.once("hidden");
  onShown = tooltip.once("shown");
  EventUtils.synthesizeMouseAtCenter(evHolder2, {},
    inspector.markup.doc.defaultView);

  await onHidden;
  info("previous tooltip hidden");

  await onShown;
  info("event tooltip for the second div is shown");

  info("Click on the computed view tab");
  let onHighlighterHidden = toolbox.once("node-unhighlight");
  let onTabComputedViewSelected = inspector.sidebar.once("computedview-selected");
  let computedViewTab = inspector.panelDoc.querySelector("#computedview-tab");
  EventUtils.synthesizeMouseAtCenter(computedViewTab, {},
    inspector.panelDoc.defaultView);

  await onTabComputedViewSelected;
  info("computed view was selected");

  await onHighlighterHidden;
  info("box model highlighter hidden after moving the mouse out of the markup view");
});
