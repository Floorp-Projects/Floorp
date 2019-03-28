/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = URL_ROOT + "doc_markup_view-original-source.html";

// Test that event handler links go to the right debugger source when the
// event handler is source mapped.
add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);

  const nodeFront = await getNodeFront("#foo", inspector);
  const container = getContainerForNodeFront(nodeFront, inspector);

  const evHolder = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]");

  evHolder.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(evHolder, {},
    inspector.markup.doc.defaultView);

  const tooltip = inspector.markup.eventDetailsTooltip;
  await tooltip.once("shown");
  await tooltip.once("event-tooltip-source-map-ready");

  const debuggerIcon = tooltip.panel.querySelector(".event-tooltip-debugger-icon");
  EventUtils.synthesizeMouse(debuggerIcon, 2, 2, {}, debuggerIcon.ownerGlobal);

  await gDevTools.showToolbox(target, "jsdebugger");
  const dbg = toolbox.getPanel("jsdebugger");

  let source;
  await BrowserTestUtils.waitForCondition(() => {
    source = dbg._selectors.getSelectedSource(dbg._getState());
    return !!source;
  }, "loaded source", 100, 20);

  is(
    source.url,
    "webpack:///events_original.js",
    "expected original source to be loaded"
  );
});
