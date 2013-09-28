/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for bug 739866.
 *
 * 1. Adds a new tab (but doesn't select it)
 * 2. Checks if timestamp on the new tab is 0
 * 3. Selects the new tab, checks that the timestamp is updated (>0)
 * 4. Selects the original tab & checks if new tab's timestamp has remained changed
 */

function test() {
  let originalTab = gBrowser.selectedTab;
  let newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  is(newTab.lastAccessed, 0, "Timestamp on the new tab is 0.");
  gBrowser.selectedTab = newTab;
  let newTabAccessedDate = newTab.lastAccessed;
  ok(newTabAccessedDate > 0, "Timestamp on the selected tab is more than 0.");
  // Date.now is not guaranteed to be monotonic, so include one second of fudge.
  let now = Date.now() + 1000;
  ok(newTabAccessedDate <= now, "Timestamp less than or equal current Date: " + newTabAccessedDate + " <= " + now);
  gBrowser.selectedTab = originalTab;
  is(newTab.lastAccessed, newTabAccessedDate, "New tab's timestamp remains the same.");
  gBrowser.removeTab(newTab);
}
