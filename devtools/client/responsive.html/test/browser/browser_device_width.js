/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  ok(ui, "An instance of the RDM should be attached to the tab.");
  yield setViewportSize(ui, manager, 110, 500);

  info("Checking initial width/height properties.");
  yield doInitialChecks(ui);

  info("Changing the RDM size");
  yield setViewportSize(ui, manager, 90, 500);

  info("Checking for screen props");
  yield checkScreenProps(ui);

  info("Changing the RDM size using input keys");
  yield setViewportSizeWithInputKeys(ui);

  info("Setting docShell.deviceSizeIsPageSize to false");
  yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    docShell.deviceSizeIsPageSize = false;
  });

  info("Checking for screen props once again.");
  yield checkScreenProps2(ui);
});

function* setViewportSizeWithInputKeys(ui) {
  let width = 320, height = 500;
  let resized = waitForViewportResizeTo(ui, width, height);
  ui.setViewportSize({ width, height });
  yield resized;

  let dimensions = ui.toolWindow.document.querySelectorAll(".viewport-dimension-input");

  // Increase width value to 420 by using the Up arrow key
  resized = waitForViewportResizeTo(ui, 420, height);
  dimensions[0].focus();
  for (let i = 1; i <= 100; i++) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
  }
  yield resized;

  // Resetting width value back to 320 using `Shift + Down` arrow
  resized = waitForViewportResizeTo(ui, width, height);
  dimensions[0].focus();
  for (let i = 1; i <= 10; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown", { shiftKey: true, code: "ArrowDown" });
  }
  yield resized;

  // Increase height value to 600 by using `PageUp + Shift` key
  resized = waitForViewportResizeTo(ui, width, 600);
  dimensions[1].focus();
  EventUtils.synthesizeKey("VK_PAGE_UP", { shiftKey: true });
  yield resized;

  // Resetting height value back to 500 by using `PageDown + Shift` key
  resized = waitForViewportResizeTo(ui, width, height);
  dimensions[1].focus();
  EventUtils.synthesizeKey("VK_PAGE_DOWN", { shiftKey: true });
  yield resized;
}

function* doInitialChecks(ui) {
  let { innerWidth, matchesMedia } = yield grabContentInfo(ui);
  is(innerWidth, 110, "initial width should be 110px");
  ok(!matchesMedia, "media query shouldn't match.");
}

function* checkScreenProps(ui) {
  let { matchesMedia, screen } = yield grabContentInfo(ui);
  ok(matchesMedia, "media query should match");
  isnot(window.screen.width, screen.width,
        "screen.width should not be the size of the screen.");
  is(screen.width, 90, "screen.width should be the page width");
  is(screen.height, 500, "screen.height should be the page height");
}

function* checkScreenProps2(ui) {
  let { matchesMedia, screen } = yield grabContentInfo(ui);
  ok(!matchesMedia, "media query should be re-evaluated.");
  is(window.screen.width, screen.width,
     "screen.width should be the size of the screen.");
}

function grabContentInfo(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return {
      screen: {
        width: content.screen.width,
        height: content.screen.height
      },
      innerWidth: content.innerWidth,
      matchesMedia: content.matchMedia("(max-device-width:100px)").matches
    };
  });
}
