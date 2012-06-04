/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let tiltOpened = false;

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
        tiltOpened = true;

        Services.obs.addObserver(finalize, DESTROYED, false);
        EventUtils.sendKey("ESCAPE");
      }
    }, false, function suddenDeath()
    {
      info("Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function finalize() {
  let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);

  is(Tilt.visualizers[id], null,
    "The current instance of the visualizer wasn't destroyed properly.");

  ok(InspectorUI.highlighter && InspectorUI.breadcrumbs,
    "The Inspector should not close while Tilt is opened.");

  cleanup();
}

function cleanup() {
  if (tiltOpened) { Services.obs.removeObserver(finalize, DESTROYED); }
  gBrowser.removeCurrentTab();
  finish();
}
