"use strict";

// Allow to open popups without any kind of interaction.
SpecialPowers.pushPrefEnv({ set: [["dom.disable_window_flip", false]] });

const FILE = getRootDirectory(gTestPath) + "open_window_in_new_tab.html";

add_task(async function() {
  info("Opening first tab: " + FILE);
  let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE);

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    FILE + "?opened",
    true
  );
  info("Opening second tab using a click");
  await ContentTask.spawn(firstTab.linkedBrowser, "", async function() {
    content.document.querySelector("#open").click();
  });

  info("Waiting for the second tab to be opened");
  let secondTab = await promiseTabOpened;

  info("Going back to the first tab");
  await BrowserTestUtils.switchTab(gBrowser, firstTab);

  info("Focusing second tab by clicking on the first tab");
  await BrowserTestUtils.switchTab(gBrowser, async function() {
    await ContentTask.spawn(firstTab.linkedBrowser, "", async function() {
      content.document.querySelector("#focus").click();
    });
  });

  is(gBrowser.selectedTab, secondTab, "Should've switched tabs");

  BrowserTestUtils.removeTab(firstTab);
  BrowserTestUtils.removeTab(secondTab);
});
