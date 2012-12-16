/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

const Cu = Components.utils;

let toolbox, target;

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

function test() {
  addTab("about:blank", function(aBrowser, aTab) {
    target = TargetFactory.forTab(gBrowser.selectedTab);
    loadWebConsole(aTab).then(function() {
      console.log('loaded');
    }, console.error);
  });
}

function loadWebConsole(aTab) {
  ok(gDevTools, "gDevTools exists");

  return gDevTools.showToolbox(target, "webconsole").then(function(aToolbox) {
    toolbox = aToolbox;
    checkToolLoading();
  }, console.error);
}

function checkToolLoading() {
  is(toolbox.currentToolId, "webconsole", "The web console is selected");
  ok(toolbox.isReady, "toolbox is ready")

  selectAndCheckById("jsdebugger").then(function() {
    selectAndCheckById("styleeditor").then(function() {
      testToggle();
    });
  }, console.error);
}

function selectAndCheckById(id) {
  let doc = toolbox.frame.contentDocument;

  return toolbox.selectTool(id).then(function() {
    let tab = doc.getElementById("toolbox-tab-" + id);
    is(tab.selected, true, "The " + id + " tab is selected");
  });
}

function testToggle() {
  toolbox.once("destroyed", function() {
    gDevTools.showToolbox(target, "styleeditor").then(function() {
      is(toolbox.currentToolId, "styleeditor", "The style editor is selected");
      finishUp();
    });
  }.bind(this));

  toolbox.destroy();
}

function finishUp() {
  toolbox.destroy();
  toolbox = null;
  target = null;
  gBrowser.removeCurrentTab();
  finish();
}
