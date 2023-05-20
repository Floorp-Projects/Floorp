/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function () {
  // The recent saves feature makes an external call to api.getpocket.com.
  // External calls are not permitted in tests.
  // however, we're not testing the content of the panel,
  // we're just testing that the right panel is used for certain urls,
  // so we can turn recent saves off for this test.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.refresh.hideRecentSaves.enabled", true]],
  });
  // Home panel is used on about: pages, so we use about:robots to test.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:robots"
  );

  const stub = sinon.stub(pktApi, "isUserLoggedIn").callsFake(() => true);

  info("clicking on pocket button in toolbar");
  let pocketButton = document.getElementById("save-to-pocket-button");
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  let pocketPanelShowing = BrowserTestUtils.waitForEvent(
    document,
    "popupshowing",
    true
  );
  pocketButton.click();
  await pocketPanelShowing;

  let pocketPanel = document.getElementById("customizationui-widget-panel");
  let pocketFrame = pocketPanel.querySelector("browser");

  await TestUtils.waitForCondition(
    () => pocketFrame.src.startsWith("about:pocket-home?"),
    "pocket home panel is showing"
  );

  info("closing pocket panel");
  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    pocketPanel,
    "popuphidden"
  );
  pocketPanel.hidePopup();
  await pocketPanelHidden;
  checkElements(false, ["customizationui-widget-panel"]);

  stub.restore();
  BrowserTestUtils.removeTab(tab);
});
