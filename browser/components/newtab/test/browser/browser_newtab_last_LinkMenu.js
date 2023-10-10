/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function setupPrefs() {
  sinon
    .stub(DiscoveryStreamFeed.prototype, "generateFeedUrl")
    .returns(
      "https://example.com/browser/browser/components/newtab/test/browser/topstories.json"
    );
  await setDefaultTopSites();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.discoverystream.config",
        JSON.stringify({
          api_key_pref: "extensions.pocket.oAuthConsumerKey",
          collapsible: true,
          enabled: true,
          personalized: false,
        }),
      ],
      [
        "browser.newtabpage.activity-stream.discoverystream.endpoints",
        "https://example.com",
      ],
    ],
  });
}

async function resetPrefs() {
  // We set 5 prefs in setupPrefs, so we should reset 5 prefs.
  // 1 popPrefEnv from pushPrefEnv
  // and 4 popPrefEnv happen internally in setDefaultTopSites.
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
}

let initialHeight;
let initialWidth;
function setSize(width, height) {
  initialHeight = window.innerHeight;
  initialWidth = window.innerWidth;
  let resizePromise = BrowserTestUtils.waitForEvent(window, "resize", false);
  window.resizeTo(width, height);
  return resizePromise;
}

function resetSize() {
  let resizePromise = BrowserTestUtils.waitForEvent(window, "resize", false);
  window.resizeTo(initialWidth, initialHeight);
  return resizePromise;
}

add_task(async function test_newtab_last_LinkMenu() {
  await setupPrefs();

  // Open about:newtab without using the default load listener
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );

  // Specially wait for potentially preloaded browsers
  let browser = tab.linkedBrowser;
  await waitForPreloaded(browser);

  // Wait for React to render something
  await BrowserTestUtils.waitForCondition(
    () =>
      SpecialPowers.spawn(
        browser,
        [],
        () => content.document.getElementById("root").children.length
      ),
    "Should render activity stream content"
  );

  // Set the window to a small enough size to trigger menus that might overflow.
  await setSize(600, 450);

  // Test context menu position for topsites.
  await SpecialPowers.spawn(browser, [], async () => {
    // Topsites might not be ready, so wait for the button.
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(
          ".top-site-outer:nth-child(2n) .context-menu-button"
        ),
      "Wait for the Pocket card and button"
    );
    const topsiteOuter = content.document.querySelector(
      ".top-site-outer:nth-child(2n)"
    );
    const topsiteContextMenuButton = topsiteOuter.querySelector(
      ".context-menu-button"
    );

    topsiteContextMenuButton.click();

    await ContentTaskUtils.waitForCondition(
      () => topsiteOuter.classList.contains("active"),
      "Wait for the menu to be active"
    );

    is(
      content.window.scrollMaxX,
      0,
      "there should be no horizontal scroll bar"
    );
  });

  // Test context menu position for topstories.
  await SpecialPowers.spawn(browser, [], async () => {
    // Pocket section might take a bit more time to load,
    // so wait for the button to be ready.
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(
          ".ds-card:nth-child(1n) .context-menu-button"
        ),
      "Wait for the Pocket card and button"
    );

    const dsCard = content.document.querySelector(".ds-card:nth-child(1n)");
    const dsCarContextMenuButton = dsCard.querySelector(".context-menu-button");

    dsCarContextMenuButton.click();

    await ContentTaskUtils.waitForCondition(
      () => dsCard.classList.contains("active"),
      "Wait for the menu to be active"
    );

    is(
      content.window.scrollMaxX,
      0,
      "there should be no horizontal scroll bar"
    );
  });

  // Resetting the window size to what it was.
  await resetSize();
  // Resetting prefs we set for this test.
  await resetPrefs();
  BrowserTestUtils.removeTab(tab);
  sinon.restore();
});
