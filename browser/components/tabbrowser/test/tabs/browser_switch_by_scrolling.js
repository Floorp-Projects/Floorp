/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function wheel_switches_tabs() {
  Services.prefs.setBoolPref("toolkit.tabbox.switchByScrolling", true);

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  await BrowserTestUtils.switchTab(gBrowser, () => {
    EventUtils.synthesizeWheel(newTab, 4, 4, {
      deltaMode: WheelEvent.DOM_DELTA_LINE,
      deltaY: -1.0,
    });
  });
  ok(!newTab.selected, "New tab should no longer be selected.");
  BrowserTestUtils.removeTab(newTab);
});

add_task(async function wheel_switches_tabs_overflow() {
  Services.prefs.setBoolPref("toolkit.tabbox.switchByScrolling", true);

  let arrowScrollbox = gBrowser.tabContainer.arrowScrollbox;
  let tabs = [];

  while (!arrowScrollbox.hasAttribute("overflowing")) {
    tabs.push(
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
    );
  }

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  await BrowserTestUtils.switchTab(gBrowser, () => {
    EventUtils.synthesizeWheel(newTab, 4, 4, {
      deltaMode: WheelEvent.DOM_DELTA_LINE,
      deltaY: -1.0,
    });
  });
  ok(!newTab.selected, "New tab should no longer be selected.");

  BrowserTestUtils.removeTab(newTab);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
