/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../debugger/test/mochitest/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const DOCUMENT_SRC = `
<body>
<button id="btn-eval">Eval</button>
<button id="btn-dom0" onclick="console.info('bloup')">DOM0</button>
<script>
var script = \`
  function foo() {
    console.log('handler');
  }
\`;
eval(script);

var button = document.getElementById("btn-eval");
button.addEventListener("click", foo, false);
</script>
</body>`;

const TEST_URI = "data:text/html;charset=utf-8," + DOCUMENT_SRC;

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  info(
    "Test that event handler links go to the right debugger source when it came from an eval()"
  );
  const evaledSource = await clickOnJumpToDebuggerIconForNode(
    inspector,
    toolbox,
    "#btn-eval"
  );
  is(evaledSource.url, null, "no expected url for eval source");

  info("Add a breakpoint in opened source");
  const debuggerContext = createDebuggerContext(toolbox);
  await addBreakpoint(
    debuggerContext,
    debuggerContext.selectors.getSelectedSource(),
    1
  );
  await safeSynthesizeMouseEventAtCenterInContentPage("#btn-eval");

  await waitForPaused(debuggerContext);
  ok(true, "The debugger paused on the evaled source breakpoint");
  await resume(debuggerContext);

  info(
    "Test that event handler links go to the right debugger source when it's a dom0 event listener."
  );
  await toolbox.selectTool("inspector");
  const dom0Source = await clickOnJumpToDebuggerIconForNode(
    inspector,
    toolbox,
    "#btn-dom0"
  );
  is(dom0Source.url, null, "no expected url for dom0 event listener source");
  await addBreakpoint(
    debuggerContext,
    debuggerContext.selectors.getSelectedSource(),
    1
  );
  await safeSynthesizeMouseEventAtCenterInContentPage("#btn-dom0");
  await waitForPaused(debuggerContext);
  ok(true, "The debugger paused on the dom0 source breakpoint");
  await resume(debuggerContext);
});

async function clickOnJumpToDebuggerIconForNode(
  inspector,
  toolbox,
  nodeSelector
) {
  const nodeFront = await getNodeFront(nodeSelector, inspector);
  const container = getContainerForNodeFront(nodeFront, inspector);

  const evHolder = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );
  evHolder.scrollIntoView();
  info(`Display event tooltip for node "${nodeSelector}"`);
  evHolder.click();

  const tooltip = inspector.markup.eventDetailsTooltip;
  await tooltip.once("shown");

  info(`Tooltip displayed, click on the "jump to debugger" icon`);
  const debuggerIcon = tooltip.panel.querySelector(
    ".event-tooltip-debugger-icon"
  );

  if (!debuggerIcon) {
    ok(
      false,
      `There is no jump to debugger icon in event tooltip for node "${nodeSelector}"`
    );
    return null;
  }

  const onDebuggerSelected = toolbox.once(`jsdebugger-selected`);
  EventUtils.synthesizeMouse(debuggerIcon, 2, 2, {}, debuggerIcon.ownerGlobal);

  const dbg = await onDebuggerSelected;
  ok(true, "The debugger was opened");

  let source;
  info("Wait for source to be opened");
  await BrowserTestUtils.waitForCondition(
    () => {
      source = dbg._selectors.getSelectedSource(dbg._getState());
      return !!source;
    },
    "loaded source",
    100,
    20
  );
  return source;
}
