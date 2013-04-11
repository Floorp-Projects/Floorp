/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let tab0, tab1;
let testStep = -1;
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

  gBrowser.tabContainer.addEventListener("TabSelect", tabSelect, false);
  createNewTab();
}

function createNewTab() {
  tab0 = gBrowser.selectedTab;

  tab1 = createTab(function() {
    Services.obs.addObserver(finalize, DESTROYED, false);
    Services.obs.addObserver(tab_STARTUP, STARTUP, false);
    Services.obs.addObserver(tab_INITIALIZING, INITIALIZING, false);
    Services.obs.addObserver(tab_DESTROYING, DESTROYING, false);
    Services.obs.addObserver(tab_SHOWN, SHOWN, false);
    Services.obs.addObserver(tab_HIDDEN, HIDDEN, false);

    info("Starting up the Tilt notifications test.");
    createTilt({
      onTiltOpen: function()
      {
        testStep = 0;
        tabSelect();
      }
    }, false, function suddenDeath()
    {
      info("Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function tab_STARTUP(win) {
  info("Handling the STARTUP notification.");
  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "STARTUP;";
}

function tab_INITIALIZING(win) {
  info("Handling the INITIALIZING notification.");
  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "INITIALIZING;";
}

function tab_DESTROYING(win) {
  info("Handling the DESTROYING notification.");
  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "DESTROYING;";
}

function tab_SHOWN(win) {
  info("Handling the SHOWN notification.");
  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "SHOWN;";
}

function tab_HIDDEN(win) {
  info("Handling the HIDDEN notification.");
  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");
  tabEvents += "HIDDEN;";
}

let testSteps = [
  function step0() {
    info("Selecting tab0.");
    gBrowser.selectedTab = tab0;
  },
  function step1() {
    info("Selecting tab1.");
    gBrowser.selectedTab = tab1;
  },
  function step2() {
    info("Killing it.");
    Tilt.destroy(Tilt.currentWindowId, true);
  }
];

function finalize(win) {
  if (!tabEvents) {
    return;
  }

  is(win, tab1.linkedBrowser.contentWindow, "Saw the correct window");

  is(tabEvents, "STARTUP;INITIALIZING;HIDDEN;SHOWN;DESTROYING;",
    "The notifications weren't fired in the correct order.");

  cleanup();
}

function cleanup() {
  info("Cleaning up the notifications test.");

  tab0 = null;
  tab1 = null;

  Services.obs.removeObserver(finalize, DESTROYED);
  Services.obs.removeObserver(tab_INITIALIZING, INITIALIZING);
  Services.obs.removeObserver(tab_DESTROYING, DESTROYING);
  Services.obs.removeObserver(tab_SHOWN, SHOWN);
  Services.obs.removeObserver(tab_HIDDEN, HIDDEN);

  gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect);
  gBrowser.removeCurrentTab();
  finish();
}

function tabSelect() {
  if (testStep !== -1) {
    executeSoon(testSteps[testStep]);
    testStep++;
  }
}
