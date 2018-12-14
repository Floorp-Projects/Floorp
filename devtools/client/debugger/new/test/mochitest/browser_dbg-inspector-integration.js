/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
  const dbg = await initDebugger("doc-script-switching.html");

  await addExpression(dbg, "window.document.body.firstChild");

  await waitForElementWithSelector(dbg, "button.open-inspector");
  findElementWithSelector(dbg, "button.open-inspector").click();

  await waitForInspectorPanelChange(dbg);
});