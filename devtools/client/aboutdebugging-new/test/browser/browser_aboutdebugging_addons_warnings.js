/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// Test that extension warnings are displayed in about:debugging.
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

  const warningText = warningMessage.textContent;
  ok(warningText.includes("wrongProperty"), "The warning message mentions wrongProperty");

  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
