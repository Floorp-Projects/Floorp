/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let newTab;

function isCurrent(tab, msg) {
  const tolerance = 5;
  const difference = Math.abs(Date.now() - tab.lastAccessed);
  ok(difference <= tolerance, msg + " (difference: " + difference + ")");
}

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.selectedTab;
  setTimeout(step2, 10);
}

function step2() {
  isCurrent(originalTab, "selected tab has the current timestamp");
  newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  setTimeout(step3, 10);
}

function step3() {
  ok(newTab.lastAccessed < Date.now(), "new tab hasn't been selected so far");
  gBrowser.selectedTab = newTab;
  isCurrent(newTab, "new tab has the current timestamp after being selected");
  setTimeout(step4, 10);
}

function step4() {
  ok(originalTab.lastAccessed < Date.now(),
     "original tab has old timestamp after being deselected");
  isCurrent(newTab, "new tab has the current timestamp since it's still selected");

  gBrowser.removeTab(newTab);
  finish();
}
