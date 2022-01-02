"use strict";

// Allow to open popups without any kind of interaction.
SpecialPowers.pushPrefEnv({ set: [["dom.disable_window_flip", false]] });

const FILE = getRootDirectory(gTestPath) + "open_window_in_new_tab.html";

add_task(async function() {
  info("Opening first tab: " + FILE);
  let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE);

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    FILE + "?open-click",
    true
  );
  info("Opening second tab using a click");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#open-click",
    {},
    firstTab.linkedBrowser
  );

  info("Waiting for the second tab to be opened");
  let secondTab = await promiseTabOpened;

  info("Going back to the first tab");
  await BrowserTestUtils.switchTab(gBrowser, firstTab);

  info("Focusing second tab by clicking on the first tab");
  await BrowserTestUtils.switchTab(gBrowser, async function() {
    await SpecialPowers.spawn(firstTab.linkedBrowser, [""], async function() {
      content.document.querySelector("#focus").click();
    });
  });

  is(gBrowser.selectedTab, secondTab, "Should've switched tabs");

  await BrowserTestUtils.removeTab(firstTab);
  await BrowserTestUtils.removeTab(secondTab);
});

add_task(async function() {
  info("Opening first tab: " + FILE);
  let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE);

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    FILE + "?open-mousedown",
    true
  );
  info("Opening second tab using a click");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#open-mousedown",
    { type: "mousedown" },
    firstTab.linkedBrowser
  );

  info("Waiting for the second tab to be opened");
  let secondTab = await promiseTabOpened;

  is(gBrowser.selectedTab, secondTab, "Should've switched tabs");

  info("Ensuring we don't switch back");
  await new Promise(resolve => {
    // We need to wait for something _not_ happening, so we need to use an arbitrary setTimeout.
    //
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(function() {
      is(gBrowser.selectedTab, secondTab, "Should've remained in original tab");
      resolve();
    }, 500);
  });

  info("cleanup");
  await BrowserTestUtils.removeTab(firstTab);
  await BrowserTestUtils.removeTab(secondTab);
});
