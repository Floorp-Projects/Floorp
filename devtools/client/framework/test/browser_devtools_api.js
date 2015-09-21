/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejections should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: this.docShell is null");

// When running in a standalone directory, we get this error
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: this.doc is undefined");

// Tests devtools API

const toolId1 = "test-tool-1";
const toolId2 = "test-tool-2";

var tempScope = {};
Cu.import("resource://gre/modules/devtools/shared/event-emitter.js", tempScope);
var EventEmitter = tempScope.EventEmitter;

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
    build: function(iframeWindow, toolbox) {
      let panel = new DevToolPanel(iframeWindow, toolbox);
      return panel.open();
    },
  };

  ok(gDevTools, "gDevTools exists");
  is(gDevTools.getToolDefinitionMap().has(toolId1), false,
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  is(gDevTools.getToolDefinitionMap().has(toolId1), true,
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
    is(toolbox._host.hostTab, gBrowser.selectedTab, "toolbox host is correct");

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
    build: function(iframeWindow, toolbox) {
      return new DevToolPanel(iframeWindow, toolbox);
    },
  };

  is(gDevTools.getToolDefinitionMap().has(toolId2), false,
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  is(gDevTools.getToolDefinitionMap().has(toolId2), true,
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
    is(toolbox._host.hostTab, gBrowser.selectedTab, "toolbox host is correct");

    ok(events["init"], "init event fired");
    ok(events["build"], "build event fired");
    ok(events["ready"], "ready event fired");

    continueTests(toolbox);
  });
}

function continueTests(toolbox, panel) {
  ok(toolbox.getCurrentPanel(), "panel value is correct");
  is(toolbox.currentToolId, toolId2, "toolbox _currentToolId is correct");

  ok(!toolbox.doc.getElementById("toolbox-tab-" + toolId2).hasAttribute("icon-invertable"),
    "The tool tab does not have the invertable attribute");

  ok(toolbox.doc.getElementById("toolbox-tab-inspector").hasAttribute("icon-invertable"),
    "The builtin tool tabs do have the invertable attribute");

  let toolDefinitions = gDevTools.getToolDefinitionMap();
  is(toolDefinitions.has(toolId2), true, "The tool is in gDevTools");

  let toolDefinition = toolDefinitions.get(toolId2);
  is(toolDefinition.id, toolId2, "toolDefinition id is correct");

  gDevTools.unregisterTool(toolId2);
  is(gDevTools.getToolDefinitionMap().has(toolId2), false,
    "The tool is no longer registered");

  // Wait for unregisterTool to select the next tool before
  // attempting to destroy.
  toolbox.on("select", function selectListener (_, id) {
    if (id !== "test-tool") {
      toolbox.off("select", selectListener);
      destroyToolbox(toolbox);
    }
  });
}

function destroyToolbox(toolbox) {
  toolbox.destroy().then(function() {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    ok(gDevTools._toolboxes.get(target) == null, "gDevTools doesn't know about target");
    ok(toolbox._target == null, "toolbox doesn't know about target.");
    finishUp();
  });
}

function finishUp() {
  tempScope = null;
  gBrowser.removeCurrentTab();
  finish();
}

/**
* When a Toolbox is started it creates a DevToolPanel for each of the tools
* by calling toolDefinition.build(). The returned object should
* at least implement these functions. They will be used by the ToolBox.
*
* There may be no benefit in doing this as an abstract type, but if nothing
* else gives us a place to write documentation.
*/
function DevToolPanel(iframeWindow, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;

  /*let doc = iframeWindow.document
  let label = doc.createElement("label");
  let textNode = doc.createTextNode("Some Tool");

  label.appendChild(textNode);
  doc.body.appendChild(label);*/
}

DevToolPanel.prototype = {
  open: function() {
    let deferred = promise.defer();

    executeSoon(() => {
      this._isReady = true;
      this.emit("ready");
      deferred.resolve(this);
    });

    return deferred.promise;
  },

  get target() {
    return this._toolbox.target;
  },

  get toolbox() {
    return this._toolbox;
  },

  get isReady() {
    return this._isReady;
  },

  _isReady: false,

  destroy: function DTI_destroy() {
    return promise.defer(null);
  },
};
