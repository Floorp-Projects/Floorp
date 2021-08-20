/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "SaveToPocket",
  "chrome://pocket/content/SaveToPocket.jsm"
);

function test_runner(test) {
  let testTask = async () => {
    // Before each
    const sandbox = sinon.createSandbox();

    // We're faking logged in tests, so initially we need to fake the logged in state.
    sandbox.stub(pktApi, "isUserLoggedIn").callsFake(() => true);

    // Also we cannot actually make remote requests, so make sure we stub any functions
    // we need that that make requests to api.getpocket.com.
    sandbox.stub(pktApi, "addLink").callsFake(() => true);

    try {
      await test({ sandbox });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

async function isPocketPanelShown() {
  info("clicking on pocket button in toolbar");
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  let pocketPanelShowing = BrowserTestUtils.waitForEvent(
    document,
    "popupshown",
    true
  );
  return pocketPanelShowing;
}

async function isPocketPanelHidden() {
  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    document,
    "popuphidden"
  );
  return pocketPanelHidden;
}

function fakeSavingPage() {
  // Because we're not actually logged into a remote Pocket account,
  // and because we're not actually saving anything,
  // we fake it, instead, by calling the function we care about.
  SaveToPocket.itemSaved();
  // This fakes the button from just opened, to also pocketed,
  // we currently expect both from a save.
  SaveToPocket.updateToolbarNodeState(window);
}

function checkPanelOpen() {
  let pocketButton = document.getElementById("save-to-pocket-button");
  // The Pocket button should be set to open.
  is(pocketButton.open, true, "Pocket button is open");
  is(pocketButton.getAttribute("pocketed"), "true", "Pocket item is pocketed");
}

function checkPanelClosed() {
  let pocketButton = document.getElementById("save-to-pocket-button");
  // Something should have closed the Pocket panel, icon should no longer be red.
  is(pocketButton.open, false, "Pocket button is closed");
  is(pocketButton.getAttribute("pocketed"), "", "Pocket item is not pocketed");
}

function synthesizeKeys(input) {
  for (const key of input.split("")) {
    EventUtils.synthesizeKey(key, {});
  }
}

test_runner(async function test_pocketButtonState_changeTabs({ sandbox }) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  let pocketPanelShown = isPocketPanelShown();
  let pocketButton = document.getElementById("save-to-pocket-button");
  pocketButton.click();
  await pocketPanelShown;
  fakeSavingPage();

  // Testing the panel states.
  checkPanelOpen();

  let pocketPanelHidden = isPocketPanelHidden();
  // Mochitests start with an open tab, so use that to trigger a tab change.
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  await pocketPanelHidden;

  // Testing the panel states.
  checkPanelClosed();

  BrowserTestUtils.removeTab(tab);
});

test_runner(async function test_pocketButtonState_changeLocation({ sandbox }) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  let pocketPanelShown = isPocketPanelShown();
  let pocketButton = document.getElementById("save-to-pocket-button");
  pocketButton.click();
  await pocketPanelShown;
  fakeSavingPage();

  // Testing the panel states.
  checkPanelOpen();

  let pocketPanelHidden = isPocketPanelHidden();
  // Trigger a url change through Ctrl+l
  EventUtils.synthesizeKey("l", { ctrlKey: true });
  synthesizeKeys("about:robots");
  EventUtils.synthesizeKey("KEY_Enter", {});
  await pocketPanelHidden;

  // Testing the panel states.
  checkPanelClosed();

  BrowserTestUtils.removeTab(tab);
});
