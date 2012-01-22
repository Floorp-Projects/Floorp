/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, executeSoon, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt, Tilt */
/*global Services, TILT_INITIALIZED, TILT_DESTROYED, TILT_SHOWN, TILT_HIDDEN */
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

  waitForExplicitFinish();

  gBrowser.tabContainer.addEventListener("TabSelect", tabSelect, false);
  createNewTab();
}

function createNewTab() {
  tab0 = gBrowser.selectedTab;

  tab1 = createTab(function() {
    Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
    Services.obs.addObserver(tab_TILT_INITIALIZED, TILT_INITIALIZED, false);
    Services.obs.addObserver(tab_TILT_DESTROYED, TILT_DESTROYED, false);
    Services.obs.addObserver(tab_TILT_SHOWN, TILT_SHOWN, false);
    Services.obs.addObserver(tab_TILT_HIDDEN, TILT_HIDDEN, false);

    createTilt({
      onTiltOpen: function()
      {
        testStep = 0;
        tabSelect();
      }
    });
  });
}

function tab_TILT_INITIALIZED() {
  tabEvents += "ti;";
}

function tab_TILT_DESTROYED() {
  tabEvents += "td;";
}

function tab_TILT_SHOWN() {
  tabEvents += "ts;";
}

function tab_TILT_HIDDEN() {
  tabEvents += "th;";
}

let testSteps = [
  function step0() {
    gBrowser.selectedTab = tab0;
  },
  function step1() {
    gBrowser.selectedTab = tab1;
  },
  function step2() {
    Tilt.destroy(Tilt.currentWindowId);
  }
];

function cleanup() {
  is(tabEvents, "ti;th;ts;td;",
    "The notifications weren't fired in the correct order.");

  tab0 = null;
  tab1 = null;

  Services.obs.removeObserver(cleanup, TILT_DESTROYED);
  Services.obs.removeObserver(tab_TILT_INITIALIZED, TILT_INITIALIZED, false);
  Services.obs.removeObserver(tab_TILT_DESTROYED, TILT_DESTROYED, false);
  Services.obs.removeObserver(tab_TILT_SHOWN, TILT_SHOWN, false);
  Services.obs.removeObserver(tab_TILT_HIDDEN, TILT_HIDDEN, false);

  gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect, false);
  gBrowser.removeCurrentTab();
  finish();
}

function tabSelect() {
  if (testStep !== -1) {
    executeSoon(testSteps[testStep]);
    testStep++;
  }
}
