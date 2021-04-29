const { InfoBar } = ChromeUtils.import(
  "resource://activity-stream/lib/InfoBar.jsm"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PROTON_ENABLED",
  "browser.proton.enabled",
  false
);

add_task(async function show_and_send_telemetry() {
  let message = (await CFRMessageProvider.getMessages()).find(
    m => m.id === "INFOBAR_ACTION_86"
  );

  Assert.ok(message.id, "Found the message");

  let dispatchStub = sinon.stub();
  let infobar = InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    {
      ...message,
      content: {
        priority: window.gNotificationBox.PRIORITY_WARNING_HIGH,
        ...message.content,
      },
    },
    dispatchStub
  );

  Assert.equal(dispatchStub.callCount, 2, "Called twice with IMPRESSION");
  // This is the call to increment impressions for frequency capping
  Assert.equal(dispatchStub.firstCall.args[0].type, "IMPRESSION");
  Assert.equal(dispatchStub.firstCall.args[0].data.id, message.id);
  // This is the telemetry ping
  Assert.equal(dispatchStub.secondCall.args[0].data.event, "IMPRESSION");
  Assert.equal(dispatchStub.secondCall.args[0].data.message_id, message.id);
  Assert.equal(
    infobar.notification.priority,
    window.gNotificationBox.PRIORITY_WARNING_HIGH,
    "Has the priority level set in the message definition"
  );

  let primaryBtn = infobar.notification.buttonContainer.querySelector(
    ".notification-button.primary"
  );

  Assert.ok(primaryBtn, "Has a primary button");
  primaryBtn.click();

  Assert.equal(dispatchStub.callCount, 4, "Called again with CLICK + removed");
  Assert.equal(dispatchStub.thirdCall.args[0].type, "USER_ACTION");
  Assert.equal(
    dispatchStub.lastCall.args[0].data.event,
    "CLICK_PRIMARY_BUTTON"
  );

  await BrowserTestUtils.waitForCondition(
    () => !InfoBar._activeInfobar,
    "Wait for notification to be dismissed by primary btn click."
  );
});

add_task(async function react_to_trigger() {
  let message = {
    ...(await CFRMessageProvider.getMessages()).find(
      m => m.id === "INFOBAR_ACTION_86"
    ),
  };
  message.targeting = "true";
  message.content.type = "tab";
  message.groups = [];
  message.provider = ASRouter.state.providers[0].id;
  message.content.message = "Infobar Mochitest";
  await ASRouter.setState({ messages: [message] });

  let notificationStack = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
  Assert.ok(
    !notificationStack.currentNotification,
    "No notification to start with"
  );

  await ASRouter.sendTriggerMessage({
    browser: BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    id: "defaultBrowserCheck",
  });

  await BrowserTestUtils.waitForCondition(
    () => notificationStack.currentNotification,
    "Wait for notification to show"
  );

  Assert.equal(
    notificationStack.currentNotification.getAttribute("value"),
    message.id,
    "Notification id should match"
  );

  let defaultPriority = PROTON_ENABLED
    ? notificationStack.PRIORITY_SYSTEM
    : notificationStack.PRIORITY_INFO_MEDIUM;
  Assert.ok(
    notificationStack.currentNotification.priority === defaultPriority,
    "Notification has default priority"
  );
  // Dismiss the notification
  notificationStack.currentNotification.closeButton.click();
});

add_task(async function dismiss_telemetry() {
  let message = {
    ...(await CFRMessageProvider.getMessages()).find(
      m => m.id === "INFOBAR_ACTION_86"
    ),
  };
  message.content.type = "tab";

  let dispatchStub = sinon.stub();
  let infobar = InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    message,
    dispatchStub
  );

  // Remove any IMPRESSION pings
  dispatchStub.reset();

  infobar.notification.closeButton.click();

  await BrowserTestUtils.waitForCondition(
    () => infobar.notification === null,
    "Set to null by `removed` event"
  );

  Assert.equal(dispatchStub.callCount, 1, "Only called once");
  Assert.equal(
    dispatchStub.firstCall.args[0].data.event,
    "DISMISSED",
    "Called with dismissed"
  );

  // Remove DISMISSED ping
  dispatchStub.reset();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  infobar = InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    message,
    dispatchStub
  );

  await BrowserTestUtils.waitForCondition(
    () => dispatchStub.callCount > 0,
    "Wait for impression ping"
  );

  // Remove IMPRESSION ping
  dispatchStub.reset();
  BrowserTestUtils.removeTab(tab);

  await BrowserTestUtils.waitForCondition(
    () => infobar.notification === null,
    "Set to null by `disconnect` event"
  );

  // Called by closing the tab and triggering "disconnect"
  Assert.equal(dispatchStub.callCount, 1, "Only called once");
  Assert.equal(
    dispatchStub.firstCall.args[0].data.event,
    "DISMISSED",
    "Called with dismissed"
  );
});

add_task(async function prevent_multiple_messages() {
  let message = (await CFRMessageProvider.getMessages()).find(
    m => m.id === "INFOBAR_ACTION_86"
  );

  Assert.ok(message.id, "Found the message");

  let dispatchStub = sinon.stub();
  let infobar = InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    message,
    dispatchStub
  );

  Assert.equal(dispatchStub.callCount, 2, "Called twice with IMPRESSION");

  // Try to stack 2 notifications
  InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    message,
    dispatchStub
  );

  Assert.equal(dispatchStub.callCount, 2, "Impression count did not increase");

  // Dismiss the first notification
  infobar.notification.closeButton.click();
  Assert.equal(InfoBar._activeInfobar, null, "Cleared the active notification");

  // Reset impressions count
  dispatchStub.reset();
  // Try show the message again
  infobar = InfoBar.showInfoBarMessage(
    BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser,
    message,
    dispatchStub
  );
  Assert.ok(InfoBar._activeInfobar, "activeInfobar is set");
  Assert.equal(dispatchStub.callCount, 2, "Called twice with IMPRESSION");
  // Dismiss the notification again
  infobar.notification.closeButton.click();
  Assert.equal(InfoBar._activeInfobar, null, "Cleared the active notification");
});
