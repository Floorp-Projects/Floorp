/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const SECURITY_DELAY = 3000;

add_task(async function () {
  // Set a custom, higher security delay for the test to avoid races on slow
  // builds.
  await SpecialPowers.pushPrefEnv({
    set: [["security.notification_enable_delay", SECURITY_DELAY]],
  });

  let notificationValue = "Protocol Registration: web+testprotocol";
  let testURI = TEST_PATH + "browser_registerProtocolHandler_notification.html";

  BrowserTestUtils.startLoadingURIString(
    window.gBrowser.selectedBrowser,
    testURI
  );
  await TestUtils.waitForCondition(
    function () {
      // Do not start until the notification is up
      let notificationBox = window.gBrowser.getNotificationBox();
      let notification =
        notificationBox.getNotificationWithValue(notificationValue);
      return notification;
    },
    "Still can not get notification after retrying 100 times.",
    100,
    100
  );

  let notificationBox = window.gBrowser.getNotificationBox();
  let notification =
    notificationBox.getNotificationWithValue(notificationValue);
  ok(notification, "Notification box should be displayed");
  if (notification == null) {
    finish();
    return;
  }
  is(
    notification.getAttribute("type"),
    "info",
    "We expect this notification to have the type of 'info'."
  );
  is(
    notification.messageImage.getAttribute("src"),
    "chrome://global/skin/icons/info-filled.svg",
    "We expect this notification to have an icon."
  );

  let buttons = notification.buttonContainer.getElementsByClassName(
    "notification-button"
  );
  is(buttons.length, 1, "We expect see one button.");

  let button = buttons[0];
  isnot(button.label, null, "We expect the add button to have a label.");
  todo(button.accesskey, "We expect the add button to have a accesskey.");

  ok(button.disabled, "We expect the button to be disabled initially.");

  let timeoutMS = SECURITY_DELAY + 100;
  info(`Wait ${timeoutMS}ms for the button to enable.`);
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, SECURITY_DELAY + 100));

  ok(
    !button.disabled,
    "We expect the button to be enabled after the security delay."
  );

  info(
    "Clicking the button should close the notification box and register the protocol."
  );

  await EventUtils.synthesizeMouseAtCenter(button, {}, window);

  await TestUtils.waitForCondition(
    () => notificationBox.currentNotification == null,
    "Waiting for notification to be closed."
  );

  info("check that the protocol handler has been registered.");
  const protoSvc = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);

  let protoInfo = protoSvc.getProtocolHandlerInfo("web+testprotocol");
  let handlers = protoInfo.possibleApplicationHandlers;
  is(1, handlers.length, "only one handler registered for web+testprotocol");
  let handler = handlers.queryElementAt(0, Ci.nsIHandlerApp);
  ok(handler instanceof Ci.nsIWebHandlerApp, "the handler is a web handler");

  // Cleanup
  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  protoInfo.preferredApplicationHandler = null;
  handlers.removeElementAt(0);
  handlerSvc.store(protoInfo);
});
