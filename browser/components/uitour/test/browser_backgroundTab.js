"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  requestLongerTimeout(2);
  UITourTest();
}

let tests = [
  function test_bg_getConfiguration(done) {
    info("getConfiguration is on the allowed list so should work");
    loadForegroundTab().then(() => {
      gContentAPI.getConfiguration("availableTargets", (data) => {
        ok(data, "Got data from getConfiguration");
        gBrowser.removeCurrentTab();
        done();
      });
    });
  },
  taskify(function* test_bg_showInfo() {
    info("showInfo isn't on the allowed action list so should be denied");
    yield loadForegroundTab();

    yield showInfoPromise("appMenu", "Hello from the backgrund", "Surprise!").then(
      () => ok(false, "panel shouldn't have shown from a background tab"),
      () => ok(true, "panel wasn't shown from a background tab"));

    gBrowser.removeCurrentTab();
  }),
];


function loadForegroundTab() {
  let newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  gBrowser.selectedTab = newTab;

  return new Promise((resolve) => {
    newTab.linkedBrowser.addEventListener("load", function onLoad() {
      newTab.linkedBrowser.removeEventListener("load", onLoad, true);
      isnot(gBrowser.selectedTab, gTestTab, "Make sure tour tab isn't selected");
      resolve();
    }, true);
  });
}
