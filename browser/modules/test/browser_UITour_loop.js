/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let loopButton;
let loopPanel = document.getElementById("loop-notification-panel");

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_menu_show_hide() {
    ise(loopButton.open, false, "Menu should initially be closed");
    gContentAPI.showMenu("loop");

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");

    ok(loopPanel.hasAttribute("noautohide"), "@noautohide should be on the loop panel");
    ok(loopPanel.hasAttribute("panelopen"), "The panel should have @panelopen");
    is(loopPanel.state, "open", "The panel should be open");
    ok(loopButton.hasAttribute("open"), "Loop button should know that the menu is open");

    gContentAPI.hideMenu("loop");
    yield waitForConditionPromise(() => {
        return !loopButton.open;
    }, "Menu should be hidden after hideMenu()");

    checkLoopPanelIsHidden();
  }),
  // Test the menu was cleaned up in teardown.
  taskify(function* setup_menu_cleanup() {
    gContentAPI.showMenu("loop");

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");

    // Leave it open so it gets torn down and we can test below that teardown was succesful.
  }),
  taskify(function* test_menu_cleanup() {
    // Test that the open menu from above was torn down fully.
    checkLoopPanelIsHidden();
  }),
];

function checkLoopPanelIsHidden() {
  ok(!loopPanel.hasAttribute("noautohide"), "@noautohide on the loop panel should have been cleaned up");
  ok(!loopPanel.hasAttribute("panelopen"), "The panel shouldn't have @panelopen");
  isnot(loopPanel.state, "open", "The panel shouldn't be open");
  is(loopButton.hasAttribute("open"), false, "Loop button should know that the panel is closed");
}

if (Services.prefs.getBoolPref("loop.enabled")) {
  loopButton = window.LoopUI.toolbarButton.node;
  registerCleanupFunction(() => {
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
} else {
  ok(true, "Loop is disabled so skip the UITour Loop tests");
  tests = [];
}
