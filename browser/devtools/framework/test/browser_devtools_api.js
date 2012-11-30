/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

const Cu = Components.utils;
const toolId = "test-tool";

let tempScope = {};
Cu.import("resource:///modules/devtools/EventEmitter.jsm", tempScope);
let EventEmitter = tempScope.EventEmitter;
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

function test() {
  addTab("about:blank", function(aBrowser, aTab) {
    runTests(aTab);
  });
}

function runTests(aTab) {
  let toolDefinition = {
    id: toolId,
    isTargetSupported: function() true,
    killswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function(iframeWindow, toolbox) {
      let panel = new DevToolPanel(iframeWindow, toolbox);
      return panel;
    },
  };

  ok(gDevTools, "gDevTools exists");
  is(gDevTools.getToolDefinitions().has(toolId), false,
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  is(gDevTools.getToolDefinitions().has(toolId), true,
    "The tool is registered");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  function onNewToolbox(event, toolboxFromEvent) {
    let toolBoxes = gDevTools._toolboxes;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let tb = toolBoxes.get(target);
    is(toolboxFromEvent, tb, "'toolbox-ready' event fired. Correct toolbox value.");
    is(tb.target, target, "toolbox target is correct");
    is(tb._host.hostTab, gBrowser.selectedTab, "toolbox host is correct");
    gDevTools.once(toolId + "-ready", continueTests);
  }

  function onToolboxClosed(event, targetFromEvent) {
    is(targetFromEvent, target, "'toolbox-destroyed' event fired. Correct tab value.");
    finishUp();
  }


  gDevTools.once("toolbox-ready", onNewToolbox);
  gDevTools.once("toolbox-destroyed", onToolboxClosed);

  executeSoon(function() {
    gDevTools.openToolbox(target, "bottom", toolId);
  });
}

function continueTests(event, toolbox, panel) {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  is(toolbox, gDevTools._toolboxes.get(target), "{toolId}-ready event received, with correct toolbox value");
  is(panel, toolbox.getToolPanels().get(toolId), "panel value is correct");

  is(toolbox.currentToolId, toolId, "toolbox _currentToolId is correct");

  let toolDefinitions = gDevTools.getToolDefinitions();
  is(toolDefinitions.has(toolId), true, "The tool is in gDevTools");

  let toolDefinition = toolDefinitions.get(toolId);
  is(toolDefinition.id, toolId, "toolDefinition id is correct");

  gDevTools.unregisterTool(toolId);
  is(gDevTools.getToolDefinitions().has(toolId), false,
    "The tool is no longer registered");

  toolbox.destroy();
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
  new EventEmitter(this);

  this._toolbox = toolbox;

  /*let doc = iframeWindow.document
  let label = doc.createElement("label");
  let textNode = doc.createTextNode("Some Tool");

  label.appendChild(textNode);
  doc.body.appendChild(label);*/

  executeSoon(function() {
    this.setReady();
  }.bind(this));
}

DevToolPanel.prototype = {
  get target() this._toolbox.target,

  get toolbox() this._toolbox,

  get isReady() this._isReady,

  _isReady: false,

  setReady: function() {
    this._isReady = true;
    this.emit("ready");
  },

  destroy: function DTI_destroy()
  {

  },
};
