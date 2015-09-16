/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var tabEvents = "";

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping notifications test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping notifications test because WebGL isn't supported.");
    return;
  }

  requestLongerTimeout(10);
  waitForExplicitFinish();

  createTab(function() {
    Services.obs.addObserver(finalize, DESTROYED, false);
    Services.obs.addObserver(obs_STARTUP, STARTUP, false);
    Services.obs.addObserver(obs_INITIALIZING, INITIALIZING, false);
    Services.obs.addObserver(obs_INITIALIZED, INITIALIZED, false);
    Services.obs.addObserver(obs_DESTROYING, DESTROYING, false);
    Services.obs.addObserver(obs_BEFORE_DESTROYED, BEFORE_DESTROYED, false);
    Services.obs.addObserver(obs_DESTROYED, DESTROYED, false);

    info("Starting up the Tilt notifications test.");
    createTilt({}, false, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function obs_STARTUP(win) {
  info("Handling the STARTUP notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "STARTUP;";
}

function obs_INITIALIZING(win) {
  info("Handling the INITIALIZING notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "INITIALIZING;";
}

function obs_INITIALIZED(win) {
  info("Handling the INITIALIZED notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "INITIALIZED;";

  Tilt.destroy(Tilt.currentWindowId, true);
}

function obs_DESTROYING(win) {
  info("Handling the DESTROYING( notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "DESTROYING;";
}

function obs_BEFORE_DESTROYED(win) {
  info("Handling the BEFORE_DESTROYED notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "BEFORE_DESTROYED;";
}

function obs_DESTROYED(win) {
  info("Handling the DESTROYED notification.");
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "DESTROYED;";
}

function finalize(win) {
  is(win, gBrowser.selectedBrowser.contentWindow, "Saw the correct window");
  is(tabEvents, "STARTUP;INITIALIZING;INITIALIZED;DESTROYING;BEFORE_DESTROYED;DESTROYED;",
    "The notifications weren't fired in the correct order.");

  cleanup();
}

function cleanup() {
  info("Cleaning up the notifications test.");

  Services.obs.removeObserver(finalize, DESTROYED);
  Services.obs.removeObserver(obs_INITIALIZING, INITIALIZING);
  Services.obs.removeObserver(obs_INITIALIZED, INITIALIZED);
  Services.obs.removeObserver(obs_DESTROYING, DESTROYING);
  Services.obs.removeObserver(obs_BEFORE_DESTROYED, BEFORE_DESTROYED);
  Services.obs.removeObserver(obs_DESTROYED, DESTROYED);
  Services.obs.removeObserver(obs_STARTUP, STARTUP);

  gBrowser.removeCurrentTab();
  finish();
}
