/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gTests = [
  test_eventMatchesKey,
  test_getTopWin,
  test_getBoolPref,
  test_openNewTabWith,
  test_openUILink
];

function test() {
  waitForExplicitFinish();
  executeSoon(runNextTest);
}

function runNextTest() {
  if (gTests.length) {
    let testFun = gTests.shift();
    info("Running " + testFun.name);
    testFun()
  } else {
    finish();
  }
}

function test_eventMatchesKey() {
  let eventMatchResult;
  let key;
  let checkEvent = function(e) {
    e.stopPropagation();
    e.preventDefault();
    eventMatchResult = eventMatchesKey(e, key);
  }
  document.addEventListener("keypress", checkEvent);

  try {
    key = document.createElement("key");
    let keyset = document.getElementById("mainKeyset");
    key.setAttribute("key", "t");
    key.setAttribute("modifiers", "accel");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("t", {accelKey: true});
    is(eventMatchResult, true, "eventMatchesKey: one modifier");
    keyset.removeChild(key);

    key = document.createElement("key");
    key.setAttribute("key", "g");
    key.setAttribute("modifiers", "accel,shift");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("g", {accelKey: true, shiftKey: true});
    is(eventMatchResult, true, "eventMatchesKey: combination modifiers");
    keyset.removeChild(key);

    key = document.createElement("key");
    key.setAttribute("key", "w");
    key.setAttribute("modifiers", "accel");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("f", {accelKey: true});
    is(eventMatchResult, false, "eventMatchesKey: mismatch keys");
    keyset.removeChild(key);

    key = document.createElement("key");
    key.setAttribute("keycode", "VK_DELETE");
    keyset.appendChild(key);
    EventUtils.synthesizeKey("VK_DELETE", {accelKey: true});
    is(eventMatchResult, false, "eventMatchesKey: mismatch modifiers");
    keyset.removeChild(key);
  } finally {
    // Make sure to remove the event listener so future tests don't
    // fail when they simulate key presses.
    document.removeEventListener("keypress", checkEvent);
  }

  runNextTest();
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
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    is(tab.linkedBrowser.currentURI.spec, "http://example.com/", "example.com loaded");
    gBrowser.removeCurrentTab();
    runNextTest();
  });
}

function test_openUILink() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    is(tab.linkedBrowser.currentURI.spec, "http://example.org/", "example.org loaded");
    gBrowser.removeCurrentTab();
    runNextTest();
  });

  openUILink("http://example.org/"); // defaults to "current"
}
