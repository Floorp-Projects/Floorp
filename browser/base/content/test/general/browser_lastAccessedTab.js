/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// gBrowser.selectedTab.lastAccessed and Date.now() called from this test can't
// run concurrently, and therefore don't always match exactly.
const CURRENT_TIME_TOLERANCE_MS = 15;

function isCurrent(tab, msg) {
  const DIFF = Math.abs(Date.now() - tab.lastAccessed);
  Assert.lessOrEqual(
    DIFF,
    CURRENT_TIME_TOLERANCE_MS,
    msg + " (difference: " + DIFF + ")"
  );
}

function nextStep(fn) {
  setTimeout(fn, CURRENT_TIME_TOLERANCE_MS + 10);
}

var originalTab;
var newTab;

function test() {
  waitForExplicitFinish();
  // This test assumes that time passes between operations. But if the precision
  // is low enough, and the test fast enough, an operation, and a successive call
  // to Date.now() will have the same time value.
  SpecialPowers.pushPrefEnv(
    { set: [["privacy.reduceTimerPrecision", false]] },
    function () {
      originalTab = gBrowser.selectedTab;
      nextStep(step2);
    }
  );
}

function step2() {
  isCurrent(originalTab, "selected tab has the current timestamp");
  newTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  nextStep(step3);
}

function step3() {
  Assert.less(
    newTab.lastAccessed,
    Date.now(),
    "new tab hasn't been selected so far"
  );
  gBrowser.selectedTab = newTab;
  isCurrent(newTab, "new tab has the current timestamp after being selected");
  nextStep(step4);
}

function step4() {
  Assert.less(
    originalTab.lastAccessed,
    Date.now(),
    "original tab has old timestamp after being deselected"
  );
  isCurrent(
    newTab,
    "new tab has the current timestamp since it's still selected"
  );

  gBrowser.removeTab(newTab);
  finish();
}
