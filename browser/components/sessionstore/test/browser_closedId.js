/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SessionStore } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionStore.jsm"
);
const BACKUP_STATE = SessionStore.getBrowserState();

async function add_new_tab(URL) {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

add_task(async function test_closedId_order() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sessionstore.restore_on_demand", true],
      ["browser.sessionstore.restore_tabs_lazily", true],
    ],
  });
  // reset to 0
  SessionStore.resetNextClosedId();
  await promiseBrowserState({
    windows: [
      {
        selected: 1, // SessionStore uses 1-based indexing.
        tabs: [
          {
            entries: [],
          },
        ],
        _closedTabs: [
          {
            state: {
              entries: [
                {
                  url: "https://www.example.com/",
                  triggeringPrincipal_base64,
                },
              ],
              selected: 1,
            },
            closedId: 0,
            closedAt: Date.now() - 100,
          },
          {
            state: {
              entries: [
                {
                  url: "about:mozilla",
                  triggeringPrincipal_base64,
                },
              ],
            },
            closedId: 1,
            closedAt: Date.now() - 50,
          },
          {
            state: {
              entries: [
                {
                  url: "https://www.example.net/",
                  triggeringPrincipal_base64,
                },
              ],
            },
            closedId: 2,
            closedAt: Date.now(),
          },
        ],
      },
    ],
  });

  let tab = await add_new_tab("about:firefoxview");

  is(
    SessionStore.getClosedTabCount(window),
    3,
    "Closed tab count after restored session is 3"
  );

  let initialClosedId = SessionStore.getClosedTabData(window)[0].closedId;

  // If this fails, that means one of the closedId's in the stubbed data in this test needs to be updated
  // to reflect what the initial closedId is when a new tab is open and closed (which may change as more tests
  // for session store are added here). You can manually verify a change to stubbed data by commenting out
  // this._resetClosedIds in SessionStore.jsm temporarily and then the "Each tab has a unique closedId" case should fail.
  is(initialClosedId, 0, "Initial closedId is 0");

  await openAndCloseTab(window, "about:robots"); // closedId should be higher than the ones we just restored.

  let closedData = SessionStore.getClosedTabData(window);
  is(closedData.length, 4, "Should have data for 4 closed tabs.");
  is(
    new Set(closedData.map(t => t.closedId)).size,
    4,
    "Each tab has a unique closedId"
  );

  BrowserTestUtils.removeTab(tab);

  // Clean up for the next task.
  await promiseBrowserState(BACKUP_STATE);
});
