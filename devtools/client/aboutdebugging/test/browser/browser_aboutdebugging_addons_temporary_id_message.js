/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// Test that temporary extensions show a message about temporary ids, with a learn more
// link.
add_task(async function() {
  const EXTENSION_NAME = "Temporary web extension";
  const EXTENSION_ID = "test-devtools@mozilla.org";

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      id: EXTENSION_ID,
      name: EXTENSION_NAME,
    },
    document
  );

  info("Wait until a debug target item appears");
  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, document));

  const target = findDebugTargetByText(EXTENSION_NAME, document);

  const message = target.querySelector(".qa-temporary-id-message");
  ok(!!message, "Temporary id message is displayed for temporary extensions");

  const link = target.querySelector(".qa-temporary-id-link");
  ok(!!link, "Temporary id link is displayed for temporary extensions");

  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});

// Test that the message and the link are not displayed for a regular extension.
add_task(async function() {
  const PACKAGED_EXTENSION_ID = "packaged-extension@tests";
  const PACKAGED_EXTENSION_NAME = "Packaged extension";

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installRegularExtension(
    "resources/packaged-extension/packaged-extension.xpi"
  );

  info("Wait until extension appears in about:debugging");
  await waitUntil(() =>
    findDebugTargetByText(PACKAGED_EXTENSION_NAME, document)
  );
  const target = findDebugTargetByText(PACKAGED_EXTENSION_NAME, document);

  const tmpIdMessage = target.querySelector(".qa-temporary-id-message");
  ok(
    !tmpIdMessage,
    "No temporary id message is displayed for a regular extension"
  );

  await removeExtension(
    PACKAGED_EXTENSION_ID,
    PACKAGED_EXTENSION_NAME,
    document
  );
  await removeTab(tab);
});
