/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOCUMENT_SRC = `
<body>
<button id="foo">Button</button>
<script>
var script = \`
  function foo() {
    console.log('handler');
  }
\`;
eval(script);

var button = document.getElementById("foo");
button.addEventListener("click", foo, false);
</script>
</body>`;

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function() {
  // Test that event handler links go to the right debugger source when it
  // came from an eval().
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);

  const nodeFront = await getNodeFront("#foo", inspector);
  const container = getContainerForNodeFront(nodeFront, inspector);

  const evHolder = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );

  evHolder.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(
    evHolder,
    {},
    inspector.markup.doc.defaultView
  );

  const tooltip = inspector.markup.eventDetailsTooltip;
  await tooltip.once("shown");

  const debuggerIcon = tooltip.panel.querySelector(
    ".event-tooltip-debugger-icon"
  );
  EventUtils.synthesizeMouse(debuggerIcon, 2, 2, {}, debuggerIcon.ownerGlobal);

  await gDevTools.showToolbox(target, "jsdebugger");
  const dbg = toolbox.getPanel("jsdebugger");

  let source;
  await BrowserTestUtils.waitForCondition(
    () => {
      source = dbg._selectors.getSelectedSource(dbg._getState());
      return !!source;
    },
    "loaded source",
    100,
    20
  );

  is(source.url, null, "expected no url for eval source");
});
