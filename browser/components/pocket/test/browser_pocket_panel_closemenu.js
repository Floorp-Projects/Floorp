/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This is testing the fix in bug 1729847, specifically
// clicking enter while the pocket panel is open should not close the panel.
add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  info("clicking on pocket button in toolbar");
  let pocketButton = document.getElementById("save-to-pocket-button");
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  let pocketPanelShown = BrowserTestUtils.waitForEvent(
    document,
    "popupshown",
    true
  );
  pocketButton.click();
  await pocketPanelShown;

  let pocketPanel = document.getElementById("customizationui-widget-panel");
  let pocketFrame = pocketPanel.querySelector("browser");

  // Ensure that the document layout has been flushed before triggering the focus event
  // (See Bug 1519808 for a rationale).
  await pocketFrame.ownerGlobal.promiseDocumentFlushed(() => {});

  // The panelview should have closemenu="none".
  // Without closemenu="none", the following sequence of
  // frame focus then enter would close the panel,
  // but we don't want it to close, we want it to stay open.
  let focusEventPromise = BrowserTestUtils.waitForEvent(pocketFrame, "focus");
  pocketFrame.focus();
  await focusEventPromise;
  EventUtils.synthesizeKey("VK_RETURN");

  // Is the Pocket panel still open?
  is(pocketPanel.state, "open", "pocket panel is open");

  // We're done now, we can close the panel.
  info("closing pocket panel");
  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    pocketPanel,
    "popuphidden"
  );
  pocketPanel.hidePopup();
  await pocketPanelHidden;

  BrowserTestUtils.removeTab(tab);
});
