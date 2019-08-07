/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test RDM menu item is checked when expected, on multiple windows.

const TEST_URL = "data:text/html;charset=utf-8,";

const isMenuCheckedFor = ({ document }) => {
  const menu = document.getElementById("menu_responsiveUI");
  return menu.getAttribute("checked") === "true";
};

add_task(async function() {
  const window1 = await BrowserTestUtils.openNewBrowserWindow();
  const { gBrowser } = window1;

  await BrowserTestUtils.withNewTab({ gBrowser, url: TEST_URL }, async function(
    browser
  ) {
    const tab = gBrowser.getTabForBrowser(browser);

    is(
      window1,
      Services.wm.getMostRecentWindow("navigator:browser"),
      "The new window is the active one"
    );

    ok(!isMenuCheckedFor(window1), "RDM menu item is unchecked by default");

    await openRDM(tab);

    ok(isMenuCheckedFor(window1), "RDM menu item is checked with RDM open");

    await closeRDM(tab);

    ok(
      !isMenuCheckedFor(window1),
      "RDM menu item is unchecked with RDM closed"
    );
  });

  await BrowserTestUtils.closeWindow(window1);

  is(
    window,
    Services.wm.getMostRecentWindow("navigator:browser"),
    "The original window is the active one"
  );

  ok(!isMenuCheckedFor(window), "RDM menu item is unchecked");
});
