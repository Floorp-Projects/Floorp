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
