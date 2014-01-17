/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function loadTestPage(callback, host = "https://example.com/") {
   if (gTestTab)
    gBrowser.removeTab(gTestTab);

  let url = getRootDirectory(gTestPath) + "uitour.html";
  url = url.replace("chrome://mochitests/content/", host);

  gTestTab = gBrowser.addTab(url);
  gBrowser.selectedTab = gTestTab;

  gTestTab.linkedBrowser.addEventListener("load", function onLoad() {
    gTestTab.linkedBrowser.removeEventListener("load", onLoad);

    gContentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
    gContentAPI = gContentWindow.Mozilla.UITour;

    waitForFocus(callback, gContentWindow);
  }, true);
}

function test() {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testUri = Services.io.newURI("http://example.com", null, null);
  Services.perms.add(testUri, "uitour", Services.perms.ALLOW_ACTION);

  waitForExplicitFinish();

  registerCleanupFunction(function() {
    delete window.UITour;
    delete window.gContentWindow;
    delete window.gContentAPI;
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    delete window.gTestTab;
    Services.prefs.clearUserPref("browser.uitour.enabled", true);
    Services.prefs.clearUserPref("services.sync.username");
    Services.perms.remove("example.com", "uitour");
  });

  function done() {
    executeSoon(() => {
      if (gTestTab)
        gBrowser.removeTab(gTestTab);
      gTestTab = null;

      let highlight = document.getElementById("UITourHighlightContainer");
      is_element_hidden(highlight, "Highlight should be closed/hidden after UITour tab is closed");

      let tooltip = document.getElementById("UITourTooltip");
      is_element_hidden(tooltip, "Tooltip should be closed/hidden after UITour tab is closed");

      is(UITour.pinnedTabs.get(window), null, "Any pinned tab should be closed after UITour tab is closed");

      executeSoon(nextTest);
    });
  }

  function nextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }
    let test = tests.shift();
    info("Starting " + test.name);
    loadTestPage(function() {
      test(done);
    });
  }
  nextTest();
}

let tests = [
  function test_checkSyncSetup_disabled(done) {
    function callback(result) {
      is(result.setup, false, "Sync shouldn't be setup by default");
      done();
    }

    gContentAPI.getSyncConfiguration(callback);
  },

  function test_checkSyncSetup_enabled(done) {
    function callback(result) {
      is(result.setup, true, "Sync should be setup");
      done();
    }

    Services.prefs.setCharPref("services.sync.username", "uitour@tests.mozilla.org");
    gContentAPI.getSyncConfiguration(callback);
  },
];
