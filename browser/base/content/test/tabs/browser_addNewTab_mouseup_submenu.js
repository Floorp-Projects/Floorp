/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test to ensure only one tab is opened when
// mousedown occurs on the newtab-button and mouseup
// occurs on the menu item.
add_task(async function addNewTab_mouseup_submenu() {
  // mouseup on the menu item
  // Only one tab should be opened
  let newTabButton = gBrowser.tabContainer.newTabButton;
  ok(newTabButton, "New tab button exists");
  ok(!newTabButton.hidden, "New tab button is visible");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    newTabButton.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(newTabButton, { type: "mousedown" });

  await popupShownPromise;
  // Because XULButtonElement will ignore the mouseup event menu
  // if it's just opened, so we need to use this setTimeout to
  // manually wait a bit.
  // See https://searchfox.org/mozilla-central/rev/77dd6aa3810610949a5ff925e24de2f8c11377fd/dom/xul/XULButtonElement.cpp#361
  await new Promise(r => setTimeout(r, 300));

  const menuItem = newTabButton.menupopup.firstChild;

  let mousemoveEvent = BrowserTestUtils.waitForEvent(menuItem, "mousemove");
  // This mousemove is needed for the following mouseup to work
  EventUtils.synthesizeMouseAtCenter(menuItem, { type: "mousemove" });
  await mousemoveEvent;

  let atLeastOneTabIsOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  EventUtils.synthesizeMouseAtCenter(menuItem, { type: "mouseup" });
  await atLeastOneTabIsOpened;

  // In a buggy build, more than one window could be opened, so let's wait
  // for all events are handled.
  await Promise.resolve();

  // The default tab + the new tab
  Assert.equal(gBrowser.tabs.length, 2);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
