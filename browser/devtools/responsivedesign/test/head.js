/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let {devtools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let TargetFactory = devtools.TargetFactory;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

function openInspector(callback)
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    callback(toolbox.getCurrentPanel());
  });
}

function openRuleView(callback)
{
  openInspector(inspector => {
    inspector.sidebar.once("ruleview-ready", () => {
      inspector.sidebar.select("ruleview");
      let ruleView = inspector.sidebar.getWindowForTab("ruleview").ruleview.view;
      callback(inspector, ruleView);
    })
  });
}
