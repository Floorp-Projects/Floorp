/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gTests = [
  test_getTopWin,
  test_getBoolPref,
  test_openNewTabWith,
  test_openUILink
];

function test () {
  waitForExplicitFinish();
  executeSoon(runNextTest);
}

function runNextTest() {
  if (gTests.length) {
    let testFun = gTests.shift();
    info("Running " + testFun.name);
    testFun()
  }
  else {
    finish();
  }
}

function test_getTopWin() {
  is(getTopWin(), window, "got top window");
  runNextTest();
}


function test_getBoolPref() {
  is(getBoolPref("browser.search.openintab", false), false, "getBoolPref");
  is(getBoolPref("this.pref.doesnt.exist", true), true, "getBoolPref fallback");
  is(getBoolPref("this.pref.doesnt.exist", false), false, "getBoolPref fallback #2");
  runNextTest();
}

function test_openNewTabWith() {
  openNewTabWith("http://example.com/");
  let tab = gBrowser.selectedTab = gBrowser.tabs[1];
  tab.linkedBrowser.addEventListener("load", function onLoad(event) {
    tab.linkedBrowser.removeEventListener("load", onLoad, true);
    is(tab.linkedBrowser.currentURI.spec, "http://example.com/", "example.com loaded");
    gBrowser.removeCurrentTab();
    runNextTest();
  }, true);
}

function test_openUILink() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  tab.linkedBrowser.addEventListener("load", function onLoad(event) {
    tab.linkedBrowser.removeEventListener("load", onLoad, true);
    is(tab.linkedBrowser.currentURI.spec, "http://example.org/", "example.org loaded");
    gBrowser.removeCurrentTab();
    runNextTest();
  }, true);

  openUILink("http://example.org/"); // defaults to "current"
}
