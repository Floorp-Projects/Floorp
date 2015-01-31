/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();
  open_preferences(runTest);
}

let gElements;

function checkElements(expectedPane) {
  for (let element of gElements) {
    // preferences elements fail is_element_visible checks because they are never visible.
    if (element.nodeName == "preferences") {
      continue;
    }
    let attributeValue = element.getAttribute("data-category");
    if (attributeValue == "pane" + expectedPane) {
      is_element_visible(element, expectedPane + " elements should be visible");
    } else {
      is_element_hidden(element, "Elements not in " + expectedPane + " should be hidden");
    }
  }
}

function runTest(win) {
  Services.prefs.setBoolPref("browser.eme.ui.enabled", true);
  is(gBrowser.currentURI.spec, "about:preferences", "about:preferences loaded");

  let tab = win.document;
  gElements = tab.getElementById("mainPrefPane").children;

  let panes = [
    "General", "Search", "Content", "Applications",
    "Privacy", "Security", "Sync", "Advanced",
  ];

  for (let pane of panes) {
    win.gotoPref("pane" + pane);
    checkElements(pane);
  }

  gBrowser.removeCurrentTab();
  win.close();
  Services.prefs.clearUserPref("browser.eme.ui.enabled");
  finish();
}
