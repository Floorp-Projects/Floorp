/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function test() {
  waitForExplicitFinish();
  open_preferences(runTest);
}

var gElements;

function checkElements(expectedPane) {
  for (let element of gElements) {
    // keyset and preferences elements fail is_element_visible checks because they are never visible.
    // special-case the drmGroup item because its visibility depends on pref + OS version
    if (element.nodeName == "keyset" ||
        element.nodeName == "preferences" ||
        element.id === "drmGroup") {
      continue;
    }
    let attributeValue = element.getAttribute("data-category");
    let suffix = " (id=" + element.id + ")";
    if (attributeValue == "pane" + expectedPane) {
      is_element_visible(element, expectedPane + " elements should be visible" + suffix);
    } else {
      is_element_hidden(element, "Elements not in " + expectedPane + " should be hidden" + suffix);
    }
  }
}

function runTest(win) {
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
  finish();
}
