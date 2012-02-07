/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping destruction test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping destruction test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onTiltOpen: function()
      {
        Services.obs.addObserver(cleanup, DESTROYED, false);
        InspectorUI.closeInspectorUI();
      }
    });
  });
}

function cleanup() {
  let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);

  is(Tilt.visualizers[id], null,
    "The current instance of the visualizer wasn't destroyed properly.");

  Services.obs.removeObserver(cleanup, DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
