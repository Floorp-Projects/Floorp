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

  info("Trigger context menu in a pocket panel element");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "context menu popup is closed");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");

  let pocketFrame = pocketPanel.querySelector("browser");

  // Avoid intermittency due to race between loading of the pocket panel
  // and our call to BrowserTestUtils.browserLoaded.
  const { readyState } = pocketFrame.contentDocument;
  if (readyState !== "complete") {
    info(
      `Wait pocket frame to be fully loaded, current readyState ${readyState}`
    );
    await BrowserTestUtils.browserLoaded(pocketFrame);
  }

  // Avoid intermittency due to synthesizeMouse not always triggering the
  // context menu as this test expects (see Bug 1519808).
  pocketFrame.contentDocument.body.dispatchEvent(
    new pocketFrame.contentWindow.MouseEvent("contextmenu", {
      bubbles: true,
      cancelable: true,
      view: pocketFrame.contentWindow,
    })
  );

  await popupShown;
  is(contextMenu.state, "open", "context menu popup is open");
  const emeLearnMoreContextItem = contextMenu.querySelector(
    "#context-media-eme-learnmore"
  );
  ok(
    BrowserTestUtils.is_hidden(emeLearnMoreContextItem),
    "Unrelated context menu items should be hidden"
  );

  contextMenu.hidePopup();
  await popupHidden;

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
