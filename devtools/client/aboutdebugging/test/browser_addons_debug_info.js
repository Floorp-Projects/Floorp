"use strict";

const { Preferences } = ChromeUtils.import("resource://gre/modules/Preferences.jsm");

const UUID_REGEX = /^([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$/;
const SHOW_SYSTEM_ADDONS_PREF = "devtools.aboutdebugging.showHiddenAddons";

function testFilePath(container, expectedFilePath) {
  // Verify that the path to the install location is shown next to its label.
  const filePath = container.querySelector(".file-path");
  ok(filePath, "file path is in DOM");
  ok(filePath.textContent.endsWith(expectedFilePath), "file path is set correctly");
  is(filePath.previousElementSibling.textContent, "Location", "file path has label");
}

add_task(async function testWebExtension() {
  const addonId = "test-devtools-webextension-nobg@mozilla.org";
  const addonName = "test-devtools-webextension-nobg";
  const { tab, document } = await openAboutDebugging("addons");

  await waitForInitialAddonList(document);

  const addonFile = ExtensionTestCommon.generateXPI({
    manifest: {
      name: addonName,
      applications: {
        gecko: {id: addonId},
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  await installAddon({
    document,
    file: addonFile,
    name: addonName,
  });

  const container = document.querySelector(`[data-addon-id="${addonId}"]`);

  testFilePath(container, addonFile.leafName);

  const extensionID = container.querySelector(".extension-id span");
  ok(extensionID.textContent === "test-devtools-webextension-nobg@mozilla.org");

  const internalUUID = container.querySelector(".internal-uuid span");
  ok(internalUUID.textContent.match(UUID_REGEX), "internalUUID is correct");

  const manifestURL = container.querySelector(".manifest-url");
  ok(manifestURL.href.startsWith("moz-extension://"), "href for manifestURL exists");

  await uninstallAddon({document, id: addonId, name: addonName});

  await closeAboutDebugging(tab);
});

add_task(async function testTemporaryWebExtension() {
  const addonName = "test-devtools-webextension-noid";
  const { tab, document } = await openAboutDebugging("addons");

  await waitForInitialAddonList(document);

  const addonFile = ExtensionTestCommon.generateXPI({
    manifest: {
      name: addonName,
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  await installAddon({
    document,
    file: addonFile,
    name: addonName,
  });

  const addons =
    document.querySelectorAll("#temporary-extensions .addon-target-container");
  // Assuming that our temporary add-on is now at the top.
  const container = addons[addons.length - 1];
  const addonId = container.dataset.addonId;

  const extensionID = container.querySelector(".extension-id span");
  ok(extensionID.textContent.endsWith("@temporary-addon"));

  const temporaryID = container.querySelector(".temporary-id-url");
  ok(temporaryID, "Temporary ID message does appear");

  await uninstallAddon({document, id: addonId, name: addonName});

  await closeAboutDebugging(tab);
});

add_task(async function testUnknownManifestProperty() {
  const addonId = "test-devtools-webextension-unknown-prop@mozilla.org";
  const addonName = "test-devtools-webextension-unknown-prop";
  const { tab, document } = await openAboutDebugging("addons");

  await waitForInitialAddonList(document);

  const addonFile = ExtensionTestCommon.generateXPI({
    manifest: {
      name: addonName,
      applications: {
        gecko: {id: addonId},
      },
      wrong_manifest_property_name: {
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  await installAddon({
    document,
    file: addonFile,
    name: addonName,
  });

  info("Wait until the addon appears in about:debugging");
  const container = await waitUntilAddonContainer(addonName, document);

  info("Wait until the installation message appears for the new addon");
  await waitUntilElement(".addon-target-messages", container);

  const messages = container.querySelectorAll(".addon-target-message");
  ok(messages.length === 1, "there is one message");
  ok(messages[0].textContent.match(/Error processing wrong_manifest_property_name/),
     "the message is helpful");
  ok(messages[0].classList.contains("addon-target-warning-message"),
     "the message is a warning");

  await uninstallAddon({document, id: addonId, name: addonName});

  await closeAboutDebugging(tab);
});

add_task(async function testSystemAddonsHidden() {
  await pushPref(SHOW_SYSTEM_ADDONS_PREF, false);

  const { document } = await openAboutDebugging("addons");
  const systemAddonsShown = () => !!document.getElementById("system-extensions");

  await waitForInitialAddonList(document);

  ok(!systemAddonsShown(), "System extensions are hidden");

  Preferences.set(SHOW_SYSTEM_ADDONS_PREF, true);

  await waitUntil(systemAddonsShown);

  ok(systemAddonsShown(), "System extensions are now shown");

  Preferences.set(SHOW_SYSTEM_ADDONS_PREF, false);

  await waitUntil(() => !systemAddonsShown());

  ok(!systemAddonsShown(), "System extensions are hidden again");
});
