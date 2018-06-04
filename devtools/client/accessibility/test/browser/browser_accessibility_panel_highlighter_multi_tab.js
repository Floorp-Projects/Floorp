/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "<h1 id=\"h1\">header</h1><p id=\"p\">paragraph</p>";

add_task(async function() {
  const { toolbox: toolbox1 } = await addTestTab(buildURL(TEST_URI));
  const { toolbox: toolbox2 } = await addTestTab(buildURL(TEST_URI));
  const options = await openOptions(toolbox2);

  info("Check that initially both accessibility panels are highlighted.");
  await checkHighlighted(toolbox1, true);
  await checkHighlighted(toolbox2, true);

  info("Toggle accessibility panel off an on.");
  await toggleAccessibility(options);
  await toggleAccessibility(options);

  await checkHighlighted(toolbox1, true);
  await checkHighlighted(toolbox2, true);

  info("Toggle accessibility panel off an on again.");
  await toggleAccessibility(options);
  await toggleAccessibility(options);

  const panel = await toolbox2.selectTool("accessibility");
  await disableAccessibilityInspector(
    { panel, win: panel.panelWin, doc: panel.panelWin.document });

  await checkHighlighted(toolbox1, false);
  await checkHighlighted(toolbox2, false);
});

async function checkHighlighted(toolbox, expected) {
  await BrowserTestUtils.waitForCondition(async function() {
    const isHighlighted = await toolbox.isToolHighlighted("accessibility");
    return isHighlighted === expected;
  });
}

async function openOptions(toolbox) {
  const panel = await toolbox.selectTool("options");
  return {
    panelWin: panel.panelWin,
    // This is a getter becuse toolbox tools list gets re-setup every time there
    // is a tool-registered or tool-undregistered event.
    get checkbox() {
      return panel.panelDoc.getElementById("accessibility");
    }
  };
}

async function toggleAccessibility({ panelWin, checkbox }) {
  const prevChecked = checkbox.checked;
  const onToggleTool = gDevTools.once(
    `tool-${prevChecked ? "unregistered" : "registered"}`);
  EventUtils.sendMouseEvent({ type: "click" }, checkbox, panelWin);
  const id = await onToggleTool;
  is(id, "accessibility", "Correct event was fired");
}
