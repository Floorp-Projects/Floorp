/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// Test that Message component can be closed with the X button
add_task(async function() {
  const EXTENSION_NAME = "Temporary web extension";
  const EXTENSION_ID = "test-devtools@mozilla.org";

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI({
    id: EXTENSION_ID,
    name: EXTENSION_NAME,
    extraProperties: {
      // This property is not expected in the manifest and should trigger a warning!
      "wrongProperty": {},
    },
  }, document);

  info("Wait until a debug target item appears");
  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, document));
  const target = findDebugTargetByText(EXTENSION_NAME, document);

  const warningMessage = target.querySelector(".qa-message");
  ok(!!warningMessage, "A warning message is displayed for the installed addon");

  const button = warningMessage.querySelector(".qa-message-button-close");
  ok(!!button, "The warning message has a close button");

  info("Closing the message and waiting for it to disappear");
  button.click();
  await waitUntil(() => {
    return target.querySelector(".qa-message") === null;
  });

  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
