/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that clicking the DOM node button in any ObjectInspect
// opens the Inspector panel

function waitForInspectorPanelChange(dbg) {
  const { toolbox } = dbg;

  return new Promise(resolve => {
    toolbox.getPanelWhenReady("inspector").then(() => {
      ok(toolbox.inspector, "Inspector is shown.");
      resolve(toolbox.inspector);
    });
  });
}

add_task(async function() {
  // Ensures the end panel is wide enough to show the inspector icon
  await pushPref("devtools.debugger.end-panel-size", 600);

  const dbg = await initDebugger("doc-script-switching.html");
  const { toolbox } = dbg;

  await addExpression(dbg, "window.document.querySelector('button')");

  await waitForElement(dbg, "openInspector");

  const inspectorNode = findElement(dbg, "openInspector");

  // Ensure hovering over button highlights the node in content pane
  const view = inspectorNode.ownerDocument.defaultView;
  const onNodeHighlight = toolbox.target.once("inspector")
    .then(inspector => inspector.highlighter.once("node-highlight"));
  EventUtils.synthesizeMouseAtCenter(inspectorNode, {type: "mousemove"}, view);
  const nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "button", "The correct node was highlighted");

  // Ensure panel changes when button is clicked
  inspectorNode.click();
  await waitForInspectorPanelChange(dbg);
});

add_task(async function() {
  const dbg = await initDebugger("doc-event-handler.html");

  invokeInTab("synthesizeClick");
  await waitForPaused(dbg);

  findElement(dbg, "frame", 2).focus();
  await clickElement(dbg, "frame", 2);

  // Hover over the token to launch preview popup
  await tryHovering(dbg, 5, 8, "popup");

  // Click the first inspector buttom to view node in inspector
  await waitForElement(dbg, "openInspector");
  findElement(dbg, "openInspector").click();

  await waitForInspectorPanelChange(dbg);
});
