/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Services, EventUtils, Tilt, TiltUtils, InspectorUI, TILT_DESTROYED */
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
        Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
        window.content.location = "about:mozilla";
      }
    });
  });
}

function cleanup() {
  let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);

  is(Tilt.visualizers[id], null,
    "The current instance of the visualizer wasn't destroyed properly.");

  Services.obs.removeObserver(cleanup, TILT_DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
