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

const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm"
);

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

function waitForDialog(callback = win => win.close()) {
  return BrowserTestUtils.promiseAlertDialog(
    null,
    "chrome://browser/content/spotlight.html",
    { callback, isSubDialog: true }
  );
}

function showAndWaitForDialog(dialogOptions, callback) {
  const promise = waitForDialog(callback);
  Spotlight.showSpotlightDialog(
    dialogOptions.browser,
    dialogOptions.message,
    dialogOptions.dispatchStub
  );
  return promise;
}

add_task(async function send_spotlight_as_page_in_telemetry() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let sandbox = sinon.createSandbox();

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    let stub = sandbox.stub(win, "AWSendEventTelemetry");
    await waitForClick("button.secondary", win);
    Assert.equal(
      stub.lastCall.args[0].event_context.page,
      "spotlight",
      "The value of event context page should be set to 'spotlight' in event telemetry"
    );
    win.close();
  });

  sandbox.restore();
});

add_task(async function send_dismiss_event_telemetry() {
  const messageId = "PB_FOCUS_PROMO"; // a multistage spotlight promo with dismiss button
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === messageId
  );
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let sandbox = sinon.createSandbox();
  let stub = sandbox.stub(AboutWelcomeTelemetry.prototype, "sendTelemetry");
  // send without a dispatch function so that default is used
  await showAndWaitForDialog({ message, browser }, async win => {
    await waitForClick("button.dismiss-button", win);
    win.close();
  });

  Assert.equal(
    stub.lastCall.args[0].message_id,
    messageId,
    "A dismiss event is called with the correct message id"
  );

  Assert.equal(
    stub.lastCall.args[0].event,
    "DISMISS",
    "A dismiss event is called with a top level event field with value 'DISMISS'"
  );

  sandbox.restore();
});

add_task(
  async function do_not_send_impression_telemetry_from_default_dispatch() {
    // Don't send impression telemetry from the Spotlight default dispatch function
    let message = (await PanelTestProvider.getMessages()).find(
      m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
    );
    let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
    let sandbox = sinon.createSandbox();
    let stub = sandbox.stub(AboutWelcomeTelemetry.prototype, "sendTelemetry");
    // send without a dispatch function so that default is used
    await showAndWaitForDialog({ message, browser });

    Assert.equal(
      stub.calledOn(),
      false,
      "No extra impression event was sent for multistage Spotlight"
    );

    sandbox.restore();
  }
);
