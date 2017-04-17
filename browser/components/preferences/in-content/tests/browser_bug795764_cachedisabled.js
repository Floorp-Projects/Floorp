/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();

  let prefs = [
    "browser.cache.offline.enable",
    "browser.cache.disk.enable",
    "browser.cache.memory.enable",
  ];

  registerCleanupFunction(function() {
    for (let pref of prefs) {
      Services.prefs.clearUserPref(pref);
    }
  });

  for (let pref of prefs) {
    Services.prefs.setBoolPref(pref, false);
  }

  open_preferences(runTest);
}

function runTest(win) {
  is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");

  let tab = win.document;
  let elements = tab.getElementById("mainPrefPane").children;

  // Test if advanced pane is opened correctly
  win.gotoPref("paneAdvanced");
  for (let element of elements) {
    if (element.nodeName == "preferences") {
      continue;
    }
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "paneAdvanced") {
      is_element_visible(element, "Advanced elements should be visible");
    } else {
      is_element_hidden(element, "Non-Advanced elements should be hidden");
    }
  }

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
