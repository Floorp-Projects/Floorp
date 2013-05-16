/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let presenter;

function test() {
  if (!isTiltEnabled()) {
    info("Skipping highlight test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping highlight test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
    let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);

    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      let contentDocument = toolbox.target.tab.linkedBrowser.contentDocument;
      let div = contentDocument.getElementById("first-law");
      toolbox.getCurrentPanel().selection.setNode(div);

      createTilt({
        onTiltOpen: function(instance)
        {
          presenter = instance.presenter;
          whenOpen();
        }
      }, false, function suddenDeath()
      {
        info("Tilt could not be initialized properly.");
        cleanup();
      });
    });
  });
}

function whenOpen() {
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
