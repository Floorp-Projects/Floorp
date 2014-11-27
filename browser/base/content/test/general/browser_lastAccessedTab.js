/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let newTab;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.selectedTab;
  setTimeout(step2, 100);
}

function step2() {
  is(originalTab.lastAccessed, Date.now(), "selected tab has the current timestamp");
  newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  setTimeout(step3, 100);
}

function step3() {
  ok(newTab.lastAccessed < Date.now(), "new tab hasn't been selected so far");
  gBrowser.selectedTab = newTab;
  is(newTab.lastAccessed, Date.now(), "new tab has the current timestamp after being selected");
  setTimeout(step4, 100);
}

function step4() {
  ok(originalTab.lastAccessed < Date.now(),
     "original tab has old timestamp after being deselected");
  is(newTab.lastAccessed, Date.now(),
     "new tab has the current timestamp since it's still selected");

  gBrowser.removeTab(newTab);
  finish();
}
