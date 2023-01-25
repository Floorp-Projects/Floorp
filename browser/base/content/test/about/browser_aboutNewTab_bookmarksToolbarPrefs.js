/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

add_task(async function test_with_different_pref_states() {
  // [prefName, prefValue, toolbarVisibleExampleCom, toolbarVisibleNewTab]
  let bookmarksToolbarVisibilityStates = [
    ["browser.toolbars.bookmarks.visibility", "newtab"],
    ["browser.toolbars.bookmarks.visibility", "always"],
    ["browser.toolbars.bookmarks.visibility", "never"],
  ];
  for (let visibilityState of bookmarksToolbarVisibilityStates) {
    await SpecialPowers.pushPrefEnv({
      set: [visibilityState],
    });

    for (let privateWin of [true, false]) {
      info(
        `Testing with ${visibilityState} in a ${
          privateWin ? "private" : "non-private"
        } window`
      );
      let win = await BrowserTestUtils.openNewBrowserWindow({
        private: privateWin,
      });
      is(
        win.gBrowser.currentURI.spec,
        privateWin ? "about:privatebrowsing" : "about:blank",
        "Expecting about:privatebrowsing or about:blank as URI of new window"
      );

      if (!privateWin) {
        await waitForBookmarksToolbarVisibility({
          win,
          visible: visibilityState[1] == "always",
          message:
            "Toolbar should be visible only if visibilityState is 'always'. State: " +
            visibilityState[1],
        });
        await BrowserTestUtils.openNewForegroundTab({
          gBrowser: win.gBrowser,
          opening: "about:newtab",
          waitForLoad: false,
        });
      }

      await waitForBookmarksToolbarVisibility({
        win,
        visible:
          visibilityState[1] == "newtab" || visibilityState[1] == "always",
        message:
          "Toolbar should be visible as long as visibilityState isn't set to 'never'. State: " +
          visibilityState[1],
      });

      await BrowserTestUtils.openNewForegroundTab({
        gBrowser: win.gBrowser,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        opening: "http://example.com",
      });
      await waitForBookmarksToolbarVisibility({
        win,
        visible: visibilityState[1] == "always",
        message:
          "Toolbar should be visible only if visibilityState is 'always'. State: " +
          visibilityState[1],
      });
      await BrowserTestUtils.closeWindow(win);
    }
  }
});
