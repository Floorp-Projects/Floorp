/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var tab0, tab1, tab2;
var testStep = -1;

function test() {
  // This test relies on a timeout to indicate pass or fail. All tests need at
  // least one pass, fail or todo so let's create a dummy pass.
  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");

  if (!isTiltEnabled()) {
    info("Skipping tab switch test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping tab switch test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  gBrowser.tabContainer.addEventListener("TabSelect", tabSelect, false);
  createTab1();
}

function createTab1() {
  tab0 = gBrowser.selectedTab;

  tab1 = createTab(function() {
    createTilt({
      onTiltOpen: function()
      {
        createTab2();
      }
    }, false, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function createTab2() {
  tab2 = createTab(function() {

    createTilt({
      onTiltOpen: function()
      {
        testStep = 0;
        tabSelect();
      }
    }, false, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

var testSteps = [
  function step0() {
    gBrowser.selectedTab = tab1;
  },
  function step1() {
    gBrowser.selectedTab = tab0;
  },
  function step2() {
    gBrowser.selectedTab = tab1;
  },
  function step3() {
    gBrowser.selectedTab = tab2;
  },
  function step4() {
    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
    tab2 = null;
  },
  function step5() {
    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
    tab1 = null;
  },
  function step6_cleanup() {
    cleanup();
  }
];

function cleanup() {
  gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect, false);

  if (tab1) {
    gBrowser.removeTab(tab1);
    tab1 = null;
  }
  if (tab2) {
    gBrowser.removeTab(tab2);
    tab2 = null;
  }

  finish();
}

function tabSelect() {
  if (testStep !== -1) {
    executeSoon(testSteps[testStep]);
    testStep++;
  }
}
