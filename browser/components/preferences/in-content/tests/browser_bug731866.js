/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

const storageManagerDisabled = !SpecialPowers.getBoolPref("browser.storageManager.enabled");
const offlineGroupDisabled = !SpecialPowers.getBoolPref("browser.preferences.offlineGroup.enabled");

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
    // The siteDataGroup in the Storage Management project is currently only pref-on on Nightly for testing purpose.
    // During the test and the transition period, we have to check the pref to see if the siteDataGroup
    // should be hidden always. This would be a bit bothersome, same as the offlineGroup as below.
    // However, this checking is necessary to make sure we don't leak the siteDataGroup into beta/release build
    if (element.id == "siteDataGroup" && storageManagerDisabled) {
      is_element_hidden(element, "Disabled siteDataGroup should be hidden");
      continue;
    }
    // The siteDataGroup in the Storage Management project will replace the offlineGroup eventually,
    // so during the transition period, we have to check the pref to see if the offlineGroup
    // should be hidden always. See the bug 1354530 for the details.
    if (element.id == "offlineGroup" && offlineGroupDisabled) {
      is_element_hidden(element, "Disabled offlineGroup should be hidden");
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
    "General", "Applications",
    "Privacy", "Sync", "Advanced",
  ];

  for (let pane of panes) {
    win.gotoPref("pane" + pane);
    checkElements(pane);
  }

  gBrowser.removeCurrentTab();
  win.close();
  finish();
}
