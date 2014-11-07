/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let tiltOpened = false;

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping destruction test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
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
        Tilt.destroy(Tilt.currentWindowId);
      }
    }, false, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function finalize() {
  let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);

  is(Tilt.visualizers[id], null,
    "The current instance of the visualizer wasn't destroyed properly.");

  cleanup();
}

function cleanup() {
  if (tiltOpened) { Services.obs.removeObserver(finalize, DESTROYED); }
  gBrowser.removeCurrentTab();
  finish();
}
