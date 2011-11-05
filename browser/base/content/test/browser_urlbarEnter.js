/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_VALUE = "example.com/\xF7?\xF7";
const START_VALUE = "example.com/%C3%B7?%C3%B7";

function test() {
  waitForExplicitFinish();
  runNextTest();
}

function locationBarEnter(aEvent, aClosure) {
  executeSoon(function() {
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", aEvent);
    addPageShowListener(aClosure);
  });
}

function runNextTest() {
  let test = gTests.shift();
  if (!test) {
    finish();
    return;
  }
  
  info("Running test: " + test.desc);
  let tab = gBrowser.selectedTab = gBrowser.addTab(START_VALUE);
  addPageShowListener(function() {
    locationBarEnter(test.event, function() {
      test.check(tab);

      // Clean up
      while (gBrowser.tabs.length > 1)
        gBrowser.removeTab(gBrowser.selectedTab)
      runNextTest();
    });
  });
}

let gTests = [
  { desc: "Simple return keypress",
    event: {},
    check: checkCurrent
  },

  { desc: "Alt+Return keypress",
    event: { altKey: true },
    check: checkNewTab,
  },
]

function checkCurrent(aTab) {
  is(gURLBar.value, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  is(gBrowser.selectedTab, aTab, "New URL was loaded in the current tab");
}

function checkNewTab(aTab) {
  is(gURLBar.value, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  isnot(gBrowser.selectedTab, aTab, "New URL was loaded in a new tab");
}

function addPageShowListener(aFunc) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    aFunc();
  });
}

