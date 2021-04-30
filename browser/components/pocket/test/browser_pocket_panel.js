/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

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

  checkElements(true, ["customizationui-widget-panel"]);
  let pocketPanel = document.getElementById("customizationui-widget-panel");
  is(pocketPanel.state, "showing", "pocket panel is showing");

  info("closing pocket panel");
  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    pocketPanel,
    "popuphidden"
  );
  pocketPanel.hidePopup();
  await pocketPanelHidden;
  checkElements(false, ["customizationui-widget-panel"]);

  BrowserTestUtils.removeTab(tab);
});
