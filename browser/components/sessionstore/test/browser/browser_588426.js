/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let state = { windows: [{ tabs: [
      {entries: [{url: "about:mozilla"}], hidden: true},
      {entries: [{url: "about:robots"}], hidden: true}
  ] }] };

  let finalState = { windows: [{ tabs: [
      {entries: [{url: "about:blank"}]}
  ] }] };

  waitForExplicitFinish();

  waitForBrowserState(state, function () {
    is(gBrowser.tabs.length, 2, "two tabs were restored");
    is(gBrowser.visibleTabs.length, 1, "one tab is visible");

    let tab = gBrowser.visibleTabs[0];
    is(tab.linkedBrowser.currentURI.spec, "about:mozilla", "visible tab is about:mozilla");

    waitForBrowserState(finalState, finish);
  });
}
