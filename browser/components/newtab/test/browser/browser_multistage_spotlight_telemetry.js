"use strict";

const { Spotlight } = ChromeUtils.import(
  "resource://activity-stream/lib/Spotlight.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
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

add_task(async function send_spotlight_as_page_in_telemetry() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let win = await showDialog({ message, browser, dispatchStub });
  let sandbox = sinon.createSandbox();

  sandbox.stub(win, "AWSendEventTelemetry");
  await waitForClick("button.secondary", win);

  Assert.equal(
    win.AWSendEventTelemetry.lastCall.args[0].event_context.page,
    "spotlight",
    "The value of event context page should be set to 'spotlight' in event telemetry"
  );

  win.close();
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
