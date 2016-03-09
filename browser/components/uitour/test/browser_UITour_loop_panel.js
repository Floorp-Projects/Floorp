"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;
var gMessageHandlers;
var loopButton;
var fakeRoom;
var loopPanel = document.getElementById("loop-notification-panel");

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
const { LoopRooms } = Cu.import("chrome://loop/content/modules/LoopRooms.jsm", {});

if (!Services.prefs.getBoolPref("loop.enabled")) {
  ok(true, "Loop is disabled so skip the UITour Loop tests");
} else {
  function checkLoopPanelIsHidden() {
    ok(!loopPanel.hasAttribute("noautohide"), "@noautohide on the loop panel should have been cleaned up");
    ok(!loopPanel.hasAttribute("panelopen"), "The panel shouldn't have @panelopen");
    isnot(loopPanel.state, "open", "The panel shouldn't be open");
    is(loopButton.hasAttribute("open"), false, "Loop button should know that the panel is closed");
  }

  add_task(setup_UITourTest);

  add_task(function() {
    loopButton = window.LoopUI.toolbarButton.node;

    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("loop.gettingStarted.latestFTUVersion");
      Services.io.offline = false;

      // Copied from browser/components/loop/test/mochitest/head.js
      // Remove the iframe after each test. This also avoids mochitest complaining
      // about leaks on shutdown as we intentionally hold the iframe open for the
      // life of the application.
      let frameId = loopButton.getAttribute("notificationFrameId");
      let frame = document.getElementById(frameId);
      if (frame) {
        frame.remove();
      }
    });
  });

  add_UITour_task(function* test_menu_show_hide() {
    // The targets to highlight only appear after getting started is launched.
    // Set latestFTUVersion to lower number to show FTU panel.
    Services.prefs.setIntPref("loop.gettingStarted.latestFTUVersion", 0);
    is(loopButton.open, false, "Menu should initially be closed");
    gContentAPI.showMenu("loop");

    yield waitForConditionPromise(() => {
      return loopPanel.state == "open";
    }, "Menu should be visible after showMenu()");

    ok(loopPanel.hasAttribute("noautohide"), "@noautohide should be on the loop panel");
    ok(loopPanel.hasAttribute("panelopen"), "The panel should have @panelopen");
    ok(loopButton.hasAttribute("open"), "Loop button should know that the menu is open");

    gContentAPI.hideMenu("loop");
    yield waitForConditionPromise(() => {
        return !loopButton.open;
    }, "Menu should be hidden after hideMenu()");

    checkLoopPanelIsHidden();
  });
}
