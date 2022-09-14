/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CLOSED_URI = "https://www.example.com/";

add_task(async function test_TODO() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, CLOSED_URI);

  let state = ss.getCurrentState(true);

  is(state.windows[0].selected, 2, "The selected tab is the second tab");

  window.FirefoxViewHandler.openTab();

  state = ss.getCurrentState(true);

  is(
    state.windows[0].selected,
    3,
    "The selected tab is Firefox view tab which is the third tab"
  );

  gBrowser.selectedTab = tab;

  state = ss.getCurrentState(true);

  // The FxView tab doesn't get recorded in the session state and when we restore we want
  // to open the tab that was previously opened so we record the tab position minus one
  is(state.windows[0].selected, 1, "The selected tab is the first tab");

  gBrowser.removeTab(window.FirefoxViewHandler.tab);
  gBrowser.removeTab(gBrowser.selectedTab);
});
