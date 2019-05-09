/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

const EXTENSION_NAME = "Temporary web extension";
const EXTENSION_ID = "test-devtools@mozilla.org";

// Test that Message component can be closed with the X button
add_task(async function() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Check that the message can be closed with icon");
  let warningMessage = await installExtensionWithWarning(document);
  await testCloseMessageWithIcon(warningMessage, document);
  await removeTemporaryExtension(EXTENSION_NAME, document);

  info("Check that the message can be closed with the button around the icon");
  warningMessage = await installExtensionWithWarning(document);
  await testCloseMessageWithButton(warningMessage, document);
  await removeTemporaryExtension(EXTENSION_NAME, document);

  await removeTab(tab);
});

async function testCloseMessageWithIcon(warningMessage, doc) {
  const closeIcon = warningMessage.querySelector(".qa-message-button-close-icon");
  ok(!!closeIcon, "The warning message has a close icon");

  info("Closing the message and waiting for it to disappear");
  closeIcon.click();

  const target = findDebugTargetByText(EXTENSION_NAME, doc);
  await waitUntil(() => target.querySelector(".qa-message") === null);
}

async function testCloseMessageWithButton(warningMessage, doc) {
  const closeButton = warningMessage.querySelector(".qa-message-button-close-button");
  ok(!!closeButton, "The warning message has a close button");

  info("Click on the button and wait for the message to disappear");
  EventUtils.synthesizeMouse(closeButton, 1, 1, {}, doc.defaultView);

  const target = findDebugTargetByText(EXTENSION_NAME, doc);
  await waitUntil(() => target.querySelector(".qa-message") === null);
}

async function installExtensionWithWarning(doc) {
  await installTemporaryExtensionFromXPI({
    id: EXTENSION_ID,
    name: EXTENSION_NAME,
    extraProperties: {
      // This property is not expected in the manifest and should trigger a warning!
      "wrongProperty": {},
    },
  }, doc);

  info("Wait until a debug target item appears");
  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, doc));

  const target = findDebugTargetByText(EXTENSION_NAME, doc);
  const warningMessage = target.querySelector(".qa-message");
  ok(!!warningMessage, "A warning message is displayed for the installed addon");

  return warningMessage;
}
