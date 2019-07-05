/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  let pageActionContextMenu = document.getElementById("pageActionPanel");
  let pageActionButton = document.getElementById("pageActionButton");
  let pageActionShown = BrowserTestUtils.waitForEvent(
    pageActionContextMenu,
    "popupshown"
  );
  let pageActionHidden = BrowserTestUtils.waitForEvent(
    pageActionContextMenu,
    "popuphidden"
  );

  info("opening page action panel");
  pageActionButton.click();
  await pageActionShown;
  checkElements(true, ["pageAction-panel-pocket"]);

  let pocketButton = document.getElementById("pageAction-panel-pocket");
  info("clicking on pageAction-panel-pocket");
  pocketButton.click();
  await pageActionHidden;

  let pocketPanel = document.getElementById("pageActionActivatedActionPanel");
  is(
    pocketPanel.state,
    "showing",
    "panel pageActionActivatedActionPanel is showing"
  );

  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    pocketPanel,
    "popuphidden"
  );
  pocketPanel.hidePopup();
  await pocketPanelHidden;
  BrowserTestUtils.removeTab(tab);
});
