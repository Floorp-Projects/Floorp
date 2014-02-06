/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let originalTab = gBrowser.selectedTab;
  isnot(originalTab.lastAccessed, 0, "selectedTab has been selected");
  ok(originalTab.lastAccessed <= Date.now(), "selectedTab has a valid timestamp");

  let newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  is(newTab.lastAccessed, 0, "newTab hasn't been selected so far");

  gBrowser.selectedTab = newTab;

  isnot(newTab.lastAccessed, 0, "newTab has been selected");
  ok(newTab.lastAccessed <= Date.now(), "newTab has a valid timestamp");

  isnot(originalTab.lastAccessed, 0, "originalTab has been selected");
  ok(originalTab.lastAccessed <= Date.now(), "originalTab has a valid timestamp");

  ok(originalTab.lastAccessed <= newTab.lastAccessed,
     "originalTab's timestamp must be lower than newTab's");

  gBrowser.removeTab(newTab);
}
