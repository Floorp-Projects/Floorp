/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function() {
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
  let pocketIframe = pocketPanel.querySelector("iframe");

  await ContentTaskUtils.waitForCondition(
    () => pocketIframe.src.split("?")[0] === "about:pocket-home",
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
