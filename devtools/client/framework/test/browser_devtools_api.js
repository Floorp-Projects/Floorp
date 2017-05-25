/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

const toolId1 = "test-tool-1";
const toolId2 = "test-tool-2";

function test() {
  addTab("about:blank").then(runTests1);
}

// Test scenario 1: the tool definition build method returns a promise.
function runTests1(aTab) {
  let toolDefinition = {
    id: toolId1,
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function (iframeWindow, toolbox) {
      let panel = createTestPanel(iframeWindow, toolbox);
      return panel.open();
    },
  };

  ok(gDevTools, "gDevTools exists");
  ok(!gDevTools.getToolDefinitionMap().has(toolId1),
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has(toolId1),
    "The tool is registered");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let events = {};

  // Check events on the gDevTools and toolbox objects.
  gDevTools.once(toolId1 + "-init", (event, toolbox, iframe) => {
    ok(iframe, "iframe argument available");

    toolbox.once(toolId1 + "-init", (event, iframe) => {
      ok(iframe, "iframe argument available");
      events["init"] = true;
    });
  });

  gDevTools.once(toolId1 + "-ready", (event, toolbox, panel) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId1 + "-ready", (event, panel) => {
      ok(panel, "panel argument available");
      events["ready"] = true;
    });
  });

  gDevTools.showToolbox(target, toolId1).then(function (toolbox) {
    is(toolbox.target, target, "toolbox target is correct");
    is(toolbox.target.tab, gBrowser.selectedTab, "targeted tab is correct");

    ok(events["init"], "init event fired");
    ok(events["ready"], "ready event fired");

    gDevTools.unregisterTool(toolId1);

    // Wait for unregisterTool to select the next tool before calling runTests2,
    // otherwise we will receive the wrong select event when waiting for
    // unregisterTool to select the next tool in continueTests below.
    toolbox.once("select", runTests2);
  });
}

// Test scenario 2: the tool definition build method returns panel instance.
function runTests2() {
  let toolDefinition = {
    id: toolId2,
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function (iframeWindow, toolbox) {
      return createTestPanel(iframeWindow, toolbox);
    },
  };

  ok(!gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is registered");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let events = {};

  // Check events on the gDevTools and toolbox objects.
  gDevTools.once(toolId2 + "-init", (event, toolbox, iframe) => {
    ok(iframe, "iframe argument available");

    toolbox.once(toolId2 + "-init", (event, iframe) => {
      ok(iframe, "iframe argument available");
      events["init"] = true;
    });
  });

  gDevTools.once(toolId2 + "-build", (event, toolbox, panel, iframe) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId2 + "-build", (event, panel, iframe) => {
      ok(panel, "panel argument available");
      events["build"] = true;
    });
  });

  gDevTools.once(toolId2 + "-ready", (event, toolbox, panel) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId2 + "-ready", (event, panel) => {
      ok(panel, "panel argument available");
      events["ready"] = true;
    });
  });

  gDevTools.showToolbox(target, toolId2).then(function (toolbox) {
    is(toolbox.target, target, "toolbox target is correct");
    is(toolbox.target.tab, gBrowser.selectedTab, "targeted tab is correct");

    ok(events["init"], "init event fired");
    ok(events["build"], "build event fired");
    ok(events["ready"], "ready event fired");

    continueTests(toolbox);
  });
}

var continueTests = Task.async(function* (toolbox, panel) {
  ok(toolbox.getCurrentPanel(), "panel value is correct");
  is(toolbox.currentToolId, toolId2, "toolbox _currentToolId is correct");

  ok(!toolbox.doc.getElementById("toolbox-tab-" + toolId2)
     .classList.contains("icon-invertable"),
     "The tool tab does not have the invertable class");

  ok(toolbox.doc.getElementById("toolbox-tab-inspector")
     .classList.contains("icon-invertable"),
     "The builtin tool tabs do have the invertable class");

  let toolDefinitions = gDevTools.getToolDefinitionMap();
  ok(toolDefinitions.has(toolId2), "The tool is in gDevTools");

  let toolDefinition = toolDefinitions.get(toolId2);
  is(toolDefinition.id, toolId2, "toolDefinition id is correct");

  info("Testing toolbox tool-unregistered event");
  let toolSelected = toolbox.once("select");
  let unregisteredTool = yield new Promise(resolve => {
    toolbox.once("tool-unregistered", (e, id) => resolve(id));
    gDevTools.unregisterTool(toolId2);
  });
  yield toolSelected;

  is(unregisteredTool, toolId2, "Event returns correct id");
  ok(!toolbox.isToolRegistered(toolId2),
    "Toolbox: The tool is not registered");
  ok(!gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is no longer registered");

  info("Testing toolbox tool-registered event");
  let registeredTool = yield new Promise(resolve => {
    toolbox.once("tool-registered", (e, id) => resolve(id));
    gDevTools.registerTool(toolDefinition);
  });

  is(registeredTool, toolId2, "Event returns correct id");
  ok(toolbox.isToolRegistered(toolId2),
    "Toolbox: The tool is registered");
  ok(gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is registered");

  info("Unregistering tool");
  gDevTools.unregisterTool(toolId2);

  destroyToolbox(toolbox);
});

function destroyToolbox(toolbox) {
  toolbox.destroy().then(function () {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    ok(gDevTools._toolboxes.get(target) == null, "gDevTools doesn't know about target");
    ok(toolbox.target == null, "toolbox doesn't know about target.");
    finishUp();
  });
}

function finishUp() {
  gBrowser.removeCurrentTab();
  finish();
}
