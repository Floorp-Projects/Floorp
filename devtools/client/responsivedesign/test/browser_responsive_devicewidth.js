/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  let tab = yield addTab("about:logo");
  let { rdm, manager } = yield openRDM(tab);
  ok(rdm, "An instance of the RDM should be attached to the tab.");
  yield setSize(rdm, manager, 110, 500);

  info("Checking initial width/height properties.");
  yield doInitialChecks();

  info("Changing the RDM size");
  yield setSize(rdm, manager, 90, 500);

  info("Checking for screen props");
  yield checkScreenProps();

  info("Setting docShell.deviceSizeIsPageSize to false");
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    docShell.deviceSizeIsPageSize = false;
  });

  info("Checking for screen props once again.");
  yield checkScreenProps2();

  yield closeRDM(rdm);
});

function* doInitialChecks() {
  let {innerWidth, matchesMedia} = yield grabContentInfo();
  is(innerWidth, 110, "initial width should be 110px");
  ok(!matchesMedia, "media query shouldn't match.");
}

function* checkScreenProps() {
  let {matchesMedia, screen} = yield grabContentInfo();
  ok(matchesMedia, "media query should match");
  isnot(window.screen.width, screen.width,
        "screen.width should not be the size of the screen.");
  is(screen.width, 90, "screen.width should be the page width");
  is(screen.height, 500, "screen.height should be the page height");
}

function* checkScreenProps2() {
  let {matchesMedia, screen} = yield grabContentInfo();
  ok(!matchesMedia, "media query should be re-evaluated.");
  is(window.screen.width, screen.width,
     "screen.width should be the size of the screen.");
}

function grabContentInfo() {
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
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
