/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { _LastSession } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs"
);

/**
 * Tests that if the user invokes undoCloseTab in a window for which there are no
 * tabs to undo closing, that we attempt to restore the previous session if one
 * exists.
 */
add_task(async function test_restore_session_in_undoCloseTab() {
  forgetClosedTabs(window);
  registerCleanupFunction(() => {
    forgetClosedTabs(window);
  });

  const state = {
    windows: [
      {
        tabs: [
          {
            entries: [
              {
                url: "https://example.org/",
                triggeringPrincipal_base64,
              },
            ],
          },
          {
            entries: [
              {
                url: "https://example.com/",
                triggeringPrincipal_base64,
              },
            ],
          },
        ],
        selected: 2,
      },
    ],
  };

  _LastSession.setState(state);

  let sessionRestored = promiseSessionStoreLoads(2 /* total restored tabs */);
  let result = undoCloseTab();
  await sessionRestored;
  Assert.equal(result, null);
  Assert.equal(gBrowser.tabs.length, 2);

  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
});
