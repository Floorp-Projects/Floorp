/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let tabEvents = "";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping notifications test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping notifications test because WebGL isn't supported.");
    return;
  }

  requestLongerTimeout(10);
  waitForExplicitFinish();

  createTab(function() {
    Services.obs.addObserver(cleanup, DESTROYED, false);

    Services.obs.addObserver(obs_INITIALIZING, INITIALIZING, false);
    Services.obs.addObserver(obs_INITIALIZED, INITIALIZED, false);
    Services.obs.addObserver(obs_DESTROYING, DESTROYING, false);
    Services.obs.addObserver(obs_BEFORE_DESTROYED, BEFORE_DESTROYED, false);
    Services.obs.addObserver(obs_DESTROYED, DESTROYED, false);

    info("Starting up the Tilt notifications test.");
    createTilt({});
  });
}

function obs_INITIALIZING() {
  info("Handling the INITIALIZING notification.");
  tabEvents += "INITIALIZING;";
}

function obs_INITIALIZED() {
  info("Handling the INITIALIZED notification.");
  tabEvents += "INITIALIZED;";

  Tilt.destroy(Tilt.currentWindowId, true);
}

function obs_DESTROYING() {
  info("Handling the DESTROYING( notification.");
  tabEvents += "DESTROYING;";
}

function obs_BEFORE_DESTROYED() {
  info("Handling the BEFORE_DESTROYED notification.");
  tabEvents += "BEFORE_DESTROYED;";
}

function obs_DESTROYED() {
  info("Handling the DESTROYED notification.");
  tabEvents += "DESTROYED;";
}

function cleanup() {
  info("Cleaning up the notifications test.");

  is(tabEvents, "INITIALIZING;INITIALIZED;DESTROYING;BEFORE_DESTROYED;DESTROYED;",
    "The notifications weren't fired in the correct order.");

  Services.obs.removeObserver(cleanup, DESTROYED);

  Services.obs.removeObserver(obs_INITIALIZING, INITIALIZING, false);
  Services.obs.removeObserver(obs_INITIALIZED, INITIALIZED, false);
  Services.obs.removeObserver(obs_DESTROYING, DESTROYING, false);
  Services.obs.removeObserver(obs_BEFORE_DESTROYED, BEFORE_DESTROYED, false);
  Services.obs.removeObserver(obs_DESTROYED, DESTROYED, false);

  gBrowser.removeCurrentTab();
  finish();
}
