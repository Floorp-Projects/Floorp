/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

Components.utils.import("resource://gre/modules/Task.jsm");
var promise = require("promise");

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping highlight test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping highlight test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    let {TargetFactory} = require("devtools/client/framework/target");
    let target = TargetFactory.forTab(gBrowser.selectedTab);

    gDevTools.showToolbox(target, "inspector")
             .then(Task.async(onToolbox));
  });
}

function* onToolbox(toolbox) {
  let contentDocument = toolbox.target.tab.linkedBrowser.contentDocument;
  let div = contentDocument.getElementById("first-law");
  let inspector = toolbox.getPanel("inspector");

  let onInspectorUpdated = inspector.once("inspector-updated");
  inspector.selection.setNode(div);
  yield onInspectorUpdated;

  let deferred = promise.defer();
  onInspectorUpdated = inspector.once("inspector-updated");
  createTilt({
    onTiltOpen: function(instance)
    {
      deferred.resolve(instance.presenter);
    }
  }, false, function suddenDeath()
  {
    ok(false, "Tilt could not be initialized properly.");
    cleanup();
  });
  let presenter = yield deferred.promise;
  yield onInspectorUpdated;

  whenOpen(presenter);
}

function whenOpen(presenter) {
  ok(presenter._currentSelection > 0,
    "Highlighting a node didn't work properly.");
  ok(!presenter._highlight.disabled,
    "After highlighting a node, it should be highlighted. D'oh.");
  ok(!presenter.controller.arcball._resetInProgress,
    "Highlighting a node that's already visible shouldn't trigger a reset.");

  executeSoon(function() {
    Services.obs.addObserver(cleanup, DESTROYED, false);
    Tilt.destroy(Tilt.currentWindowId);
  });
}

function cleanup() {
  Services.obs.removeObserver(cleanup, DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
