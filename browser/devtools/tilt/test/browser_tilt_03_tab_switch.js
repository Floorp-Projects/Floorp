/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, executeSoon, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt, Tilt */
"use strict";

let tab0, tab1, tab2;
let testStep = -1;

function test() {
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
      onInspectorOpen: function()
      {
        ok(Tilt.tiltButton.checked === false,
          "The toolbar tilt button shouldn't be pressed before Tilt is open.");
      },
      onTiltOpen: function()
      {
        createTab2();
      }
    });
  });
}

function createTab2() {
  tab2 = createTab(function() {

    createTilt({
      onInspectorOpen: function()
      {
        ok(Tilt.tiltButton.checked === false,
          "The toolbar tilt button shouldn't be pressed before Tilt is open.");
      },
      onTiltOpen: function()
      {
        testStep = 0;
        tabSelect();
      }
    });
  });
}

let testSteps = [
  function step0() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step0 (tab 2).");

    gBrowser.selectedTab = tab1;
  },
  function step1() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step1 (tab 1).");

    gBrowser.selectedTab = tab0;
  },
  function step2() {
    ok(Tilt.tiltButton.checked === false,
      "The toolbar tilt button shouldn't have been pressed at step2 (tab 0).");

    gBrowser.selectedTab = tab1;
  },
  function step3() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step3 (tab 1).");

    gBrowser.selectedTab = tab2;
  },
  function step4() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step4 (tab 2).");

    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
  },
  function step5() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step5 (tab 1).");

    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
  },
  function step6_cleanup() {
    ok(Tilt.tiltButton.checked === false,
      "The toolbar tilt button shouldn't have been pressed at step6 (tab 0).");

    tab1 = null;
    tab2 = null;

    gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect, false);
    finish();
  }
];

function tabSelect() {
  if (testStep !== -1) {
    executeSoon(testSteps[testStep]);
    testStep++;
  }
}
