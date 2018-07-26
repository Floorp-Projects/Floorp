/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

"use strict";

const toolId1 = "test-tool-1";
const toolId2 = "test-tool-2";

function test() {
  addTab("about:blank").then(runTests1);
}

// Test scenario 1: the tool definition build method returns a promise.
function runTests1(tab) {
  const toolDefinition = {
    id: toolId1,
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function(iframeWindow, toolbox) {
      const panel = createTestPanel(iframeWindow, toolbox);
      return panel.open();
    },
  };

  ok(gDevTools, "gDevTools exists");
  ok(!gDevTools.getToolDefinitionMap().has(toolId1),
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has(toolId1),
    "The tool is registered");

  const target = TargetFactory.forTab(gBrowser.selectedTab);

  const events = {};

  // Check events on the gDevTools and toolbox objects.
  gDevTools.once(toolId1 + "-init", (toolbox, iframe) => {
    ok(iframe, "iframe argument available");

    toolbox.once(toolId1 + "-init", innerIframe => {
      ok(innerIframe, "innerIframe argument available");
      events.init = true;
    });
  });

  gDevTools.once(toolId1 + "-ready", (toolbox, panel) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId1 + "-ready", innerPanel => {
      ok(innerPanel, "innerPanel argument available");
      events.ready = true;
    });
  });

  gDevTools.showToolbox(target, toolId1).then(function(toolbox) {
    is(toolbox.target, target, "toolbox target is correct");
    is(toolbox.target.tab, gBrowser.selectedTab, "targeted tab is correct");

    ok(events.init, "init event fired");
    ok(events.ready, "ready event fired");

    gDevTools.unregisterTool(toolId1);

    // Wait for unregisterTool to select the next tool before calling runTests2,
    // otherwise we will receive the wrong select event when waiting for
    // unregisterTool to select the next tool in continueTests below.
    toolbox.once("select", runTests2);
  });
}

// Test scenario 2: the tool definition build method returns panel instance.
function runTests2() {
  const toolDefinition = {
    id: toolId2,
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function(iframeWindow, toolbox) {
      return createTestPanel(iframeWindow, toolbox);
    },
  };

  ok(!gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is registered");

  const target = TargetFactory.forTab(gBrowser.selectedTab);

  const events = {};

  // Check events on the gDevTools and toolbox objects.
  gDevTools.once(toolId2 + "-init", (toolbox, iframe) => {
    ok(iframe, "iframe argument available");

    toolbox.once(toolId2 + "-init", innerIframe => {
      ok(innerIframe, "innerIframe argument available");
      events.init = true;
    });
  });

  gDevTools.once(toolId2 + "-build", (toolbox, panel, iframe) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId2 + "-build", (innerPanel, innerIframe) => {
      ok(innerPanel, "innerPanel argument available");
      events.build = true;
    });
  });

  gDevTools.once(toolId2 + "-ready", (toolbox, panel) => {
    ok(panel, "panel argument available");

    toolbox.once(toolId2 + "-ready", innerPanel => {
      ok(innerPanel, "innerPanel argument available");
      events.ready = true;
    });
  });

  gDevTools.showToolbox(target, toolId2).then(function(toolbox) {
    is(toolbox.target, target, "toolbox target is correct");
    is(toolbox.target.tab, gBrowser.selectedTab, "targeted tab is correct");

    ok(events.init, "init event fired");
    ok(events.build, "build event fired");
    ok(events.ready, "ready event fired");

    continueTests(toolbox);
  });
}

var continueTests = async function(toolbox, panel) {
  ok(toolbox.getCurrentPanel(), "panel value is correct");
  is(toolbox.currentToolId, toolId2, "toolbox _currentToolId is correct");

  const toolDefinitions = gDevTools.getToolDefinitionMap();
  ok(toolDefinitions.has(toolId2), "The tool is in gDevTools");

  const toolDefinition = toolDefinitions.get(toolId2);
  is(toolDefinition.id, toolId2, "toolDefinition id is correct");

  info("Testing toolbox tool-unregistered event");
  const toolSelected = toolbox.once("select");
  const unregisteredTool = await new Promise(resolve => {
    toolbox.once("tool-unregistered", id => resolve(id));
    gDevTools.unregisterTool(toolId2);
  });
  await toolSelected;

  is(unregisteredTool, toolId2, "Event returns correct id");
  ok(!toolbox.isToolRegistered(toolId2),
    "Toolbox: The tool is not registered");
  ok(!gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is no longer registered");

  info("Testing toolbox tool-registered event");
  const registeredTool = await new Promise(resolve => {
    toolbox.once("tool-registered", id => resolve(id));
    gDevTools.registerTool(toolDefinition);
  });

  is(registeredTool, toolId2, "Event returns correct id");
  ok(toolbox.isToolRegistered(toolId2),
    "Toolbox: The tool is registered");
  ok(gDevTools.getToolDefinitionMap().has(toolId2),
    "The tool is registered");

  info("Unregistering tool");
  gDevTools.unregisterTool(toolId2);

  info("Destroying toolbox");
  destroyToolbox(toolbox);
};

function destroyToolbox(toolbox) {
  toolbox.destroy().then(function() {
    const target = TargetFactory.forTab(gBrowser.selectedTab);
    ok(gDevTools._toolboxes.get(target) == null, "gDevTools doesn't know about target");
    ok(toolbox.target == null, "toolbox doesn't know about target.");
    finishUp();
  });
}

function finishUp() {
  gBrowser.removeCurrentTab();
  finish();
}
