/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Spotlight } = ChromeUtils.import(
  "resource://activity-stream/lib/Spotlight.jsm"
);
const { PanelTestProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/PanelTestProvider.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

async function showDialog(dialogOptions) {
  Spotlight.showSpotlightDialog(
    dialogOptions.browser,
    dialogOptions.message,
    dialogOptions.dispatchStub
  );
  const [win] = await TestUtils.topicObserved("subdialog-loaded");
  return win;
}

add_task(async function test_specialAction() {
  const sandbox = sinon.createSandbox();
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  let dispatchStub = sandbox.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let specialActionStub = sandbox.stub(SpecialMessageActions, "handleAction");

  let win = await showDialog({ message, browser, dispatchStub });
  await waitForClick("button.primary", win);
  win.close();

  Assert.equal(
    specialActionStub.callCount,
    1,
    "Should be called by primary action"
  );
  Assert.deepEqual(
    specialActionStub.firstCall.args[0],
    message.content.screens[0].content.primary_button.action,
    "Should be called with button action"
  );

  sandbox.restore();
});

add_task(async function test_embedded_import() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.internal-testing.enabled", true]],
  });
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "IMPORT_SETTINGS_EMBEDDED"
  );
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let win = await showDialog({ message, browser });
  let migrationWizardReady = BrowserTestUtils.waitForEvent(
    win,
    "MigrationWizard:Ready"
  );

  await TestUtils.waitForCondition(() =>
    win.document.querySelector("migration-wizard")
  );
  Assert.ok(
    win.document.querySelector("migration-wizard"),
    "Migration Wizard rendered"
  );

  await migrationWizardReady;

  let panelList = win.document
    .querySelector("migration-wizard")
    .openOrClosedShadowRoot.querySelector("panel-list");
  Assert.equal(panelList.tagName, "PANEL-LIST");
  Assert.equal(panelList.firstChild.tagName, "PANEL-ITEM");

  win.close();
  await SpecialPowers.popPrefEnv();
});
