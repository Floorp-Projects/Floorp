/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf8,test for dynamically registering " +
            "and unregistering tools across multiple tabs";

let tab1, tab2, modifiedPref;

add_task(async function() {
  tab1 = await openToolboxOptionsInNewTab();
  tab2 = await openToolboxOptionsInNewTab();

  await testToggleTools();
  await cleanup();
});

async function openToolboxOptionsInNewTab() {
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target);
  const doc = toolbox.doc;
  const panel = await toolbox.selectTool("options");
  const { id } = panel.panelDoc.querySelector(
    "#default-tools-box input[type=checkbox]:not([data-unsupported]):not([checked])");

  return {
    tab,
    toolbox,
    doc,
    panelWin: panel.panelWin,
    // This is a getter becuse toolbox tools list gets re-setup every time there
    // is a tool-registered or tool-undregistered event.
    get checkbox() {
      return panel.panelDoc.getElementById(id);
    }
  };
}

async function testToggleTools() {
  is(tab1.checkbox.id, tab2.checkbox.id, "Default tool box should be in sync.");

  const toolId = tab1.checkbox.id;
  const testTool = gDevTools.getDefaultTools().find(tool => tool.id === toolId);
  // Store modified pref names so that they can be cleared on error.
  modifiedPref = testTool.visibilityswitch;

  info(`Registering tool ${toolId} in the first tab.`);
  await toggleTool(tab1, toolId);

  info(`Unregistering tool ${toolId} in the first tab.`);
  await toggleTool(tab1, toolId);

  info(`Registering tool ${toolId} in the second tab.`);
  await toggleTool(tab2, toolId);

  info(`Unregistering tool ${toolId} in the second tab.`);
  await toggleTool(tab2, toolId);

  info(`Registering tool ${toolId} in the first tab.`);
  await toggleTool(tab1, toolId);

  info(`Unregistering tool ${toolId} in the second tab.`);
  await toggleTool(tab2, toolId);
}

async function toggleTool({ doc, panelWin, checkbox, tab }, toolId) {
  const prevChecked = checkbox.checked;

  (prevChecked ? checkRegistered : checkUnregistered)(toolId);

  const onToggleTool = gDevTools.once(
    `tool-${prevChecked ? "unregistered" : "registered"}`);
  EventUtils.sendMouseEvent({ type: "click" }, checkbox, panelWin);
  const id = await onToggleTool;

  is(id, toolId, `Correct event for ${toolId} was fired`);
  // await new Promise(resolve => setTimeout(resolve, 60000));
  (prevChecked ? checkUnregistered : checkRegistered)(toolId);
}

async function checkUnregistered(toolId) {
  ok(!tab1.doc.getElementById("toolbox-tab-" + toolId),
    `Tab for unregistered tool ${toolId} is not present in first toolbox`);
  ok(!tab1.checkbox.checked,
    `Checkbox for unregistered tool ${toolId} is not checked in first toolbox`);
  ok(!tab2.doc.getElementById("toolbox-tab-" + toolId),
    `Tab for unregistered tool ${toolId} is not present in second toolbox`);
  ok(!tab2.checkbox.checked,
    `Checkbox for unregistered tool ${toolId} is not checked in second toolbox`);
}

function checkRegistered(toolId) {
  ok(tab1.doc.getElementById("toolbox-tab-" + toolId),
    `Tab for registered tool ${toolId} is present in first toolbox`);
  ok(tab1.checkbox.checked,
    `Checkbox for registered tool ${toolId} is checked in first toolbox`);
  ok(tab2.doc.getElementById("toolbox-tab-" + toolId),
    `Tab for registered tool ${toolId} is present in second toolbox`);
  ok(tab2.checkbox.checked,
    `Checkbox for registered tool ${toolId} is checked in second toolbox`);
}

async function cleanup() {
  await tab1.toolbox.destroy();
  await tab2.toolbox.destroy();
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(modifiedPref);
  tab1 = tab2 = modifiedPref = null;
}
