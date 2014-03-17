/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let {devtools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let TargetFactory = devtools.TargetFactory;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

function openInspector(callback)
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    callback(toolbox.getCurrentPanel());
  });
}

function openView(name, callback)
{
  openInspector(inspector => {
    function onReady() {
      inspector.sidebar.select(name);
      let { view } = inspector.sidebar.getWindowForTab(name)[name];
      callback(inspector, view);
    }

    if (inspector.sidebar.getTab(name)) {
      onReady();
    } else {
      inspector.sidebar.once(name + "-ready", onReady);
    }
  });
}
