/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

/**
 * Test that the installation error messages are displayed when installing temporary
 * extensions.
 */

const INVALID_JSON_EXTENSION_PATH =
  "resources/bad-extensions/invalid-json/manifest.json";
const INVALID_PROP_EXTENSION_PATH =
  "resources/bad-extensions/invalid-property/manifest.json";
const EXTENSION_PATH = "resources/test-temporary-extension/manifest.json";
const EXTENSION_NAME = "test-temporary-extension";

// Extension with an invalid JSON manifest will not be parsed. We check the expected
// error message is displayed
add_task(async function testInvalidJsonExtension() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const installError = await installBadExtension(
    INVALID_JSON_EXTENSION_PATH,
    document
  );
  ok(
    installError.textContent.includes("JSON.parse: unexpected keyword"),
    "The expected installation error is displayed: " + installError.textContent
  );

  info("Install a valid extension to make the message disappear");
  await installTemporaryExtension(EXTENSION_PATH, EXTENSION_NAME, document);

  info("Wait until the error message disappears");
  await waitUntil(
    () => !document.querySelector(".qa-tmp-extension-install-error")
  );

  info("Wait for the temporary addon to be displayed as a debug target");
  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, document));

  await removeTemporaryExtension(EXTENSION_NAME, document);

  await removeTab(tab);
});

// Extension with a valid JSON manifest but containing an invalid property should display
// a detailed error message coming from the Addon Manager.
add_task(async function testInvalidPropertyExtension() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const installError = await installBadExtension(
    INVALID_PROP_EXTENSION_PATH,
    document
  );

  ok(
    installError.textContent.includes("Extension is invalid"),
    "The basic installation error is displayed: " + installError.textContent
  );
  ok(
    installError.textContent.includes(
      "Reading manifest: Error processing content_scripts.0.matches"
    ),
    "The detailed installation error is also displayed: " +
      installError.textContent
  );

  await removeTab(tab);
});

async function installBadExtension(path, document) {
  info("Install a bad extension at path: " + path);
  // Do not use installTemporaryAddon here since the install will fail.
  prepareMockFilePicker(path);
  document.querySelector(".qa-temporary-extension-install-button").click();

  info("Wait until the install error message appears");
  await waitUntil(() =>
    document.querySelector(".qa-tmp-extension-install-error")
  );
  return document.querySelector(".qa-tmp-extension-install-error");
}
