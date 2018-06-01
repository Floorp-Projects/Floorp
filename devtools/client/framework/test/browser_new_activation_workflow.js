/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

var toolbox, target;

function test() {
  addTab("about:blank").then(function(aTab) {
    target = TargetFactory.forTab(gBrowser.selectedTab);
    loadWebConsole(aTab).then(function() {
      console.log("loaded");
    });
  });
}

function loadWebConsole(aTab) {
  ok(gDevTools, "gDevTools exists");

  return gDevTools.showToolbox(target, "webconsole").then(function(aToolbox) {
    toolbox = aToolbox;
    checkToolLoading();
  });
}

function checkToolLoading() {
  is(toolbox.currentToolId, "webconsole", "The web console is selected");
  ok(toolbox.isReady, "toolbox is ready");

  selectAndCheckById("jsdebugger").then(function() {
    selectAndCheckById("styleeditor").then(function() {
      testToggle();
    });
  });
}

function selectAndCheckById(id) {
  return toolbox.selectTool(id).then(function() {
    const tab = toolbox.doc.getElementById("toolbox-tab-" + id);
    is(tab.classList.contains("selected"), true, "The " + id + " tab is selected");
    is(tab.getAttribute("aria-pressed"), "true", "The " + id + " tab is pressed");
  });
}

function testToggle() {
  toolbox.once("destroyed", () => {
    // Cannot reuse a target after it's destroyed.
    target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "styleeditor").then(function(aToolbox) {
      toolbox = aToolbox;
      is(toolbox.currentToolId, "styleeditor", "The style editor is selected");
      finishUp();
    });
  });

  toolbox.destroy();
}

function finishUp() {
  toolbox.destroy().then(function() {
    toolbox = null;
    target = null;
    gBrowser.removeCurrentTab();
    finish();
  });
}
