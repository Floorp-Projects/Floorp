/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
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
        ok(InspectorUI.inspecting,
          "The Inspector should be inspecting for tab1.");
        ok(InspectorUI.highlighter.hidden === false,
          "The Highlighter should be visible for tab1.");
      },
      onTiltOpen: function()
      {
        ok(InspectorUI.inspecting === false,
          "The Inspector should not be inspecting for tab1 after Tilt is open.");
        ok(InspectorUI.highlighter.hidden,
          "The Highlighter should not be visible for tab1 after Tilt is open.");

        createTab2();
      }
    }, false, function suddenDeath()
    {
      info("Tilt could not be initialized properly.");
      cleanup();
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
        ok(InspectorUI.inspecting,
          "The Inspector should be inspecting for tab2.");
        ok(InspectorUI.highlighter.hidden === false,
          "The Highlighter should be visible for tab2.");
      },
      onTiltOpen: function()
      {
        ok(InspectorUI.inspecting === false,
          "The Inspector should not be inspecting for tab2 after Tilt is open.");
        ok(InspectorUI.highlighter.hidden,
          "The Highlighter should be visible for tab2 after Tilt is open.");

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

let testSteps = [
  function step0() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step0 (tab 2).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step0.");
    ok(InspectorUI.highlighter.hidden,
      "The Highlighter should be hidden at step0.");

    gBrowser.selectedTab = tab1;
  },
  function step1() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step1 (tab 1).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step1.");
    ok(InspectorUI.highlighter.hidden,
      "The Highlighter should be hidden at step1.");

    gBrowser.selectedTab = tab0;
  },
  function step2() {
    ok(Tilt.tiltButton.checked === false,
      "The toolbar tilt button shouldn't have been pressed at step2 (tab 0).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step2.");
    is(InspectorUI.highlighter, null,
      "The Highlighter should be dead while in step2.");

    gBrowser.selectedTab = tab1;
  },
  function step3() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step3 (tab 1).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step3.");
    ok(InspectorUI.highlighter.hidden,
      "The Highlighter should be hidden at step3.");

    gBrowser.selectedTab = tab2;
  },
  function step4() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step4 (tab 2).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step4.");
    ok(InspectorUI.highlighter.hidden,
      "The Highlighter should be hidden at step4.");

    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
    tab2 = null;
  },
  function step5() {
    ok(Tilt.tiltButton.checked === true,
      "The toolbar tilt button should have been pressed at step5 (tab 1).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step5.");
    ok(InspectorUI.highlighter.hidden,
      "The Highlighter should be hidden at step5.");

    Tilt.destroy(Tilt.currentWindowId);
    gBrowser.removeCurrentTab();
    tab1 = null;
  },
  function step6_cleanup() {
    ok(Tilt.tiltButton.checked === false,
      "The toolbar tilt button shouldn't have been pressed at step6 (tab 0).");
    ok(InspectorUI.inspecting === false,
      "The Inspector should not be inspecting at step6.");
    is(InspectorUI.highlighter, null,
      "The Highlighter should be dead while in step6.");

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
