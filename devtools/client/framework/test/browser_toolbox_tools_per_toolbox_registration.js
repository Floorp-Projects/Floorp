/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = `data:text/html,<!DOCTYPE html>
  <html>
    <head>
      <meta charset="utf-8">
    </head>
    <body>
      test for registering and unregistering tools to a specific toolbox
    </body>
  </html>`;

const TOOL_ID = "test-toolbox-tool";
var toolbox;
var target;

function test() {
  addTab(TEST_URL).then(tab => {
    target = TargetFactory.forTab(tab);

    gDevTools.showToolbox(target)
      .then(toolboxRegister)
      .then(testToolRegistered);
  });
}

var resolveToolInstanceBuild;
var waitForToolInstanceBuild = new Promise((resolve) => {
  resolveToolInstanceBuild = resolve;
});

var resolveToolInstanceDestroyed;
var waitForToolInstanceDestroyed = new Promise((resolve) => {
  resolveToolInstanceDestroyed = resolve;
});

function toolboxRegister(aToolbox) {
  toolbox = aToolbox;

  var resolveToolInstanceBuild;

  waitForToolInstanceBuild = new Promise((resolve) => {
    resolveToolInstanceBuild = resolve;
  });

  info("add per-toolbox tool in the opened toolbox.");

  toolbox.addAdditionalTool({
    id: TOOL_ID,
    label: "per-toolbox Test Tool",
    inMenu: true,
    isTargetSupported: () => true,
    build: function () {
      info("per-toolbox tool has been built.");
      resolveToolInstanceBuild();

      return {
        destroy: () => {
          info("per-toolbox tool has been destroyed.");
          resolveToolInstanceDestroyed();
        },
      };
    },
    key: "t"
  });
}

function testToolRegistered() {
  ok(!gDevTools.getToolDefinitionMap().has(TOOL_ID), "per-toolbox tool is not registered globally");
  ok(toolbox.hasAdditionalTool(TOOL_ID),
     "per-toolbox tool registered to the specific toolbox");

  // Test that the tool appeared in the UI.
  let doc = toolbox.doc;
  let tab = doc.getElementById("toolbox-tab-" + TOOL_ID);
  ok(tab, "new tool's tab exists in toolbox UI");

  let panel = doc.getElementById("toolbox-panel-" + TOOL_ID);
  ok(panel, "new tool's panel exists in toolbox UI");

  for (let win of getAllBrowserWindows()) {
    let key = win.document.getElementById("key_" + TOOL_ID);
    if (win.document == doc) {
      continue;
    }
    ok(!key, "key for new tool should not exists in the other browser windows");
    let menuitem = win.document.getElementById("menuitem_" + TOOL_ID);
    ok(!menuitem, "menu item should not exists in the other browser window");
  }

  // Test that the tool is built once selected and then test its unregistering.
  info("select per-toolbox tool in the opened toolbox.");
  gDevTools.showToolbox(target, TOOL_ID)
           .then(waitForToolInstanceBuild)
           .then(testUnregister);
}

function getAllBrowserWindows() {
  let wins = [];
  let enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    wins.push(enumerator.getNext());
  }
  return wins;
}

function testUnregister() {
  info("remove per-toolbox tool in the opened toolbox.");
  toolbox.removeAdditionalTool(TOOL_ID);

  Promise.all([
    waitForToolInstanceDestroyed
  ]).then(toolboxToolUnregistered);
}

function toolboxToolUnregistered() {
  ok(!toolbox.hasAdditionalTool(TOOL_ID),
     "per-toolbox tool unregistered from the specific toolbox");

  // test that it disappeared from the UI
  let doc = toolbox.doc;
  let tab = doc.getElementById("toolbox-tab-" + TOOL_ID);
  ok(!tab, "tool's tab was removed from the toolbox UI");

  let panel = doc.getElementById("toolbox-panel-" + TOOL_ID);
  ok(!panel, "tool's panel was removed from toolbox UI");

  cleanup();
}

function cleanup() {
  toolbox.destroy();
  toolbox = null;
  gBrowser.removeCurrentTab();
  finish();
}
