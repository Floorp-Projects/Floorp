/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const testCases = [
  {
    name: "bookmarks_toolbar_shown_on_newtab_newTabEnabled",
    newTabEnabled: true,
  },
  {
    name: "bookmarks_toolbar_shown_on_newtab",
    newTabEnabled: false,
  },
];

async function test_bookmarks_toolbar_visibility({ newTabEnabled }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", newTabEnabled]],
  });

  // Ensure the toolbar doesnt become visible at any point before the tab finishes loading

  let url =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "slow_loading_page.sjs";

  let startTime = Date.now();
  let newWindowOpened = BrowserTestUtils.domWindowOpened();
  let beforeShown = TestUtils.topicObserved("browser-window-before-show");

  openTrustedLinkIn(url, "window");

  let newWin = await newWindowOpened;
  let slowSiteLoaded = BrowserTestUtils.firstBrowserLoaded(newWin, false);

  function checkToolbarIsCollapsed(win, message) {
    let toolbar = win.document.getElementById("PersonalToolbar");
    ok(toolbar && toolbar.collapsed, message);
  }

  await beforeShown;
  checkToolbarIsCollapsed(
    newWin,
    "Toolbar is initially hidden on the new window"
  );

  function onToolbarMutation() {
    checkToolbarIsCollapsed(newWin, "Toolbar should remain collapsed");
  }
  let toolbarMutationObserver = new newWin.MutationObserver(onToolbarMutation);
  toolbarMutationObserver.observe(
    newWin.document.getElementById("PersonalToolbar"),
    {
      attributeFilter: ["collapsed"],
    }
  );

  info("Waiting for the slow site to load");
  await slowSiteLoaded;
  info(`Window opened and slow site loaded in: ${Date.now() - startTime}ms`);

  checkToolbarIsCollapsed(newWin, "Finally, the toolbar is still hidden");

  toolbarMutationObserver.disconnect();
  await BrowserTestUtils.closeWindow(newWin);
}

// Make separate tasks for each test case, so we get more useful stack traces on failure
for (let testData of testCases) {
  let tmp = {
    async [testData.name]() {
      info("testing with: " + JSON.stringify(testData));
      await test_bookmarks_toolbar_visibility(testData);
    },
  };
  add_task(tmp[testData.name]);
}
