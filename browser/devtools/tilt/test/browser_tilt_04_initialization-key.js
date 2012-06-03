/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let id;
let tiltKey;
let eventType;

function test() {
  if (!isTiltEnabled()) {
    info("Skipping initialization key test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping initialization key test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);
  tiltKey = Tilt.tiltButton.getAttribute("accesskey");

  if ("nsILocalFileMac" in Ci) {
    eventType = { type: "keypress", ctrlKey: true };
  } else {
    eventType = { type: "keypress", altKey: true };
  }

  Services.obs.addObserver(onInspectorOpen, INSPECTOR_OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function suddenDeath() {
  Services.obs.removeObserver(onTiltOpen, INITIALIZING);
  cleanup();
}

function onInspectorOpen() {
  Services.obs.removeObserver(onInspectorOpen, INSPECTOR_OPENED);

  executeSoon(function() {
    is(Tilt.visualizers[id], null,
      "A instance of the visualizer shouldn't be initialized yet.");

    info("Pressing the accesskey should open Tilt.");

    Tilt.failureCallback = suddenDeath;

    Services.obs.addObserver(onTiltOpen, INITIALIZING, false);
    EventUtils.synthesizeKey(tiltKey, eventType);
  });
}

function onTiltOpen() {
  Services.obs.removeObserver(onTiltOpen, INITIALIZING);

  executeSoon(function() {
    ok(Tilt.visualizers[id] instanceof TiltVisualizer,
      "A new instance of the visualizer wasn't created properly.");
    ok(Tilt.visualizers[id].isInitialized(),
      "The new instance of the visualizer wasn't initialized properly.");

    info("Pressing the accesskey again should close Tilt.");

    Services.obs.addObserver(onTiltClose, DESTROYED, false);
    EventUtils.synthesizeKey(tiltKey, eventType);
  });
}

function onTiltClose() {
  is(Tilt.visualizers[id], null,
    "The current instance of the visualizer wasn't destroyed properly.");

  cleanup();
}

function cleanup() {
  Tilt.failureCallback = null;
  InspectorUI.closeInspectorUI();
  finish();
}
