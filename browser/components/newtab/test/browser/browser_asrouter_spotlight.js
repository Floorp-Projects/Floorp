/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
const { SpecialMessageActions } = ChromeUtils.import(
  "resource://messaging-system/lib/SpecialMessageActions.jsm"
);

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

add_task(async function test_show_spotlight() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "SPOTLIGHT_MESSAGE_93"
  );

  Assert.ok(message?.id, "Should find the Spotlight message");

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    win.document.getElementById("secondary").click();
  });

  Assert.ok(!gBrowser.ownerGlobal.gDialogBox.isOpen, "Should close Spotlight");
});

add_task(async function test_telemetry() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "SPOTLIGHT_MESSAGE_93"
  );

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    win.document.getElementById("secondary").click();
  });

  Assert.equal(
    dispatchStub.callCount,
    3,
    "1 IMPRESSION and 2 SPOTLIGHT_TELEMETRY"
  );
  Assert.equal(
    dispatchStub.firstCall.args[0].type,
    "SPOTLIGHT_TELEMETRY",
    "Should match"
  );
  Assert.equal(
    dispatchStub.firstCall.args[0].data.event,
    "IMPRESSION",
    "Should match"
  );
  Assert.equal(
    dispatchStub.secondCall.args[0].type,
    "IMPRESSION",
    "Should match"
  );
  Assert.equal(
    dispatchStub.thirdCall.args[0].type,
    "SPOTLIGHT_TELEMETRY",
    "Should match"
  );
  Assert.equal(
    dispatchStub.thirdCall.args[0].data.event,
    "DISMISS",
    "Should match"
  );
});

add_task(async function test_primaryButton() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "SPOTLIGHT_MESSAGE_93"
  );

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let specialActionStub = sinon.stub(SpecialMessageActions, "handleAction");

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    win.document.getElementById("primary").click();
  });

  Assert.equal(
    specialActionStub.callCount,
    1,
    "Should be called by primary action"
  );
  Assert.deepEqual(
    specialActionStub.firstCall.args[0],
    message.content.body.primary.action,
    "Should be called with button action"
  );

  Assert.equal(
    dispatchStub.lastCall.args[0].data.event,
    "CLICK",
    "Should send dismiss telemetry"
  );

  specialActionStub.restore();
});

add_task(async function test_secondaryButton() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "SPOTLIGHT_MESSAGE_93"
  );

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
  let specialActionStub = sinon.stub(SpecialMessageActions, "handleAction");

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    win.document.getElementById("secondary").click();
  });

  Assert.equal(
    specialActionStub.callCount,
    1,
    "Should be called by primary action"
  );
  Assert.deepEqual(
    specialActionStub.firstCall.args[0],
    message.content.body.secondary.action,
    "Should be called with secondary action"
  );

  Assert.equal(
    dispatchStub.lastCall.args[0].data.event,
    "DISMISS",
    "Should send dismiss telemetry"
  );

  specialActionStub.restore();
});

add_task(async function test_remoteL10n_content() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "SPOTLIGHT_MESSAGE_93"
  );

  // Modify the message to mix translated and un-translated content
  message = {
    ...message,
    content: {
      ...message.content,
      body: {
        ...message.content.body,
        secondary: {
          ...message.content.body.secondary,
          label: "Changed Label",
        },
      },
    },
  };

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    await win.document.mozSubdialogReady;
    let primaryBtn = win.document.getElementById("primary");
    let secondaryBtn = win.document.getElementById("secondary");
    Assert.ok(
      primaryBtn.getElementsByTagName("remote-text").length,
      "Primary button should have a remote l10n element"
    );
    Assert.ok(
      secondaryBtn.getElementsByTagName("remote-text").length === 0,
      "Secondary button should not have a remote l10n element"
    );
    Assert.equal(
      primaryBtn.getElementsByTagName("remote-text")[0].shadowRoot.textContent,
      "Stay private with Mozilla VPN",
      "Should have expected strings for primary btn"
    );
    Assert.equal(
      secondaryBtn.getElementsByTagName("span")[0].textContent,
      "Changed Label",
      "Should have expected strings for secondary btn"
    );

    // Dismiss
    win.document.getElementById("secondary").click();
  });
});

add_task(async function test_contentExpanded() {
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "TCP_SPOTLIGHT_MESSAGE_95"
  );

  let dispatchStub = sinon.stub();
  let browser = BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;

  await showAndWaitForDialog({ message, browser, dispatchStub }, async win => {
    const toggle = win.document.getElementById("learn-more-toggle");
    Assert.equal(
      toggle.getAttribute("aria-expanded"),
      "false",
      "Toggle initially collapsed"
    );
    toggle.click();
    Assert.equal(
      toggle.getAttribute("aria-expanded"),
      "true",
      "Toggle switched to expanded"
    );
    win.close();
  });
});
