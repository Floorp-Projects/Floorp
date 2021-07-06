/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "SaveToPocket",
  "chrome://pocket/content/SaveToPocket.jsm"
);

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  // We're faking a logged in test, so initially we need to fake the logged in state.
  const loggedInStub = sinon
    .stub(pktApi, "isUserLoggedIn")
    .callsFake(() => true);
  // Also we cannot actually make remote requests, so make sure we stub any functions
  // we need that that make requests to api.getpocket.com.
  const addLinkStub = sinon.stub(pktApi, "addLink").callsFake(() => true);

  info("clicking on pocket button in toolbar");
  let pocketButton = document.getElementById("save-to-pocket-button");
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  let pocketPanelShowing = BrowserTestUtils.waitForEvent(
    document,
    "popupshown",
    true
  );
  pocketButton.click();
  await pocketPanelShowing;

  // Because we're not actually logged into a remote Pocket account,
  // and because we're not actually saving anything,
  // we fake it, instead, by calling the function we care about.
  SaveToPocket.itemSaved();
  // This fakes the button from just opened, to also pocketed,
  // we currently expect both from a save.
  SaveToPocket.updateToolbarNodeState(window);

  // The Pocket button should be set to open.
  is(pocketButton.open, true, "Pocket button is open");
  is(pocketButton.getAttribute("pocketed"), "true", "Pocket item is pocketed");

  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    document,
    "popuphidden"
  );

  // Mochitests start with an open tab, so use that to trigger a tab change.
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);

  await pocketPanelHidden;

  // Opening a new tab should have closed the Pocket panel, icon should no longer be red.
  is(pocketButton.open, false, "Pocket button is closed");
  is(pocketButton.getAttribute("pocketed"), "", "Pocket item is not pocketed");

  loggedInStub.restore();
  addLinkStub.restore();
  BrowserTestUtils.removeTab(tab);
});
