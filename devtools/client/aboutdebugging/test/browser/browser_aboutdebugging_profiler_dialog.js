/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/performance-new/test/browser/helpers.js",
  this
);

const BackgroundJSM = ChromeUtils.import(
  "resource://devtools/client/performance-new/shared/background.jsm.js"
);

registerCleanupFunction(() => {
  BackgroundJSM.revertRecordingSettings();
});

const RUNTIME_ID = "1337id";
const DEVICE_NAME = "Fancy Phone";
const RUNTIME_NAME = "Lorem ipsum";

/**
 * Test opening and closing the profiler dialog.
 */
add_task(async function test_opening_profiler_dialog() {
  const { mocks } = await connectToLocalFirefox();
  const { document, tab, window } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);

  info("Open the profiler dialog");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Click on the close button and wait until the dialog disappears");
  const closeDialogButton = document.querySelector(".qa-profiler-dialog-close");
  closeDialogButton.click();
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Click on the mask element and wait until the dialog disappears");
  const mask = document.querySelector(".qa-profiler-dialog-mask");
  EventUtils.synthesizeMouse(mask, 5, 5, {}, window);
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Open the profiler dialog again");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  info("Navigate to this-firefox and wait until the dialog disappears");
  document.location.hash = "#/runtime/this-firefox";
  await waitUntil(() => !document.querySelector(".qa-profiler-dialog"));
  assertDialogHidden(document);

  info("Select the remote runtime again, check the dialog is still hidden");
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);
  assertDialogHidden(document);

  await disconnectFromLocalFirefox({ mocks, doc: document });
  await removeTab(tab);
});

add_task(async function test_set_profiler_settings() {
  const { mocks } = await connectToLocalFirefox();
  const { document, tab } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);

  info("Open the profiler dialog");
  await openProfilerDialogWithRealClient(document);
  assertDialogVisible(document);

  const profilerSettingsDocument = await openProfilerSettings(document);
  const radioButtonForCustomPreset = await getNearestInputFromText(
    profilerSettingsDocument,
    "Custom"
  );
  ok(
    radioButtonForCustomPreset.checked,
    "The radio button for the preset 'custom' is checked."
  );

  info("Change the preset to Graphics.");
  const radioButtonForGraphicsPreset = await getNearestInputFromText(
    profilerSettingsDocument,
    "Graphics"
  );
  radioButtonForGraphicsPreset.click();

  const profilerDocument = await saveSettingsAndGoBack(document);
  const perfPresetsSelect = await getNearestInputFromText(
    profilerDocument,
    "Settings"
  );
  is(
    perfPresetsSelect.value,
    "graphics",
    "The preset has been changed in the devtools panel UI as well."
  );

  await disconnectFromLocalFirefox({ mocks, doc: document });
  await removeTab(tab);
});

async function connectToLocalFirefox() {
  // This is a client to the current Firefox.
  const clientWrapper = await createLocalClientWrapper();

  // enable USB devices mocks
  const mocks = new Mocks();
  const usbClient = mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: DEVICE_NAME,
    name: RUNTIME_NAME,
    clientWrapper,
  });

  return { mocks, usbClient };
}

async function disconnectFromLocalFirefox({ doc, mocks }) {
  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, doc);
}

function assertDialogVisible(doc) {
  ok(doc.querySelector(".qa-profiler-dialog"), "Dialog is displayed");
  ok(doc.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is displayed");
}

function assertDialogHidden(doc) {
  ok(!doc.querySelector(".qa-profiler-dialog"), "Dialog is removed");
  ok(!doc.querySelector(".qa-profiler-dialog-mask"), "Dialog mask is removed");
}

/**
 * Retrieve the iframe containing the profiler UIs.
 * Be careful as it's completely replaced when switching UIs.
 */
function getProfilerIframe(doc) {
  return doc.querySelector(".profiler-dialog__frame");
}

/**
 * This waits for the full render of the UI inside the profiler iframe, and
 * returns the content document object.
 */
async function waitForProfilerUiRendering(doc, selector) {
  // The iframe is replaced completely, so we need to retrieve a new reference
  // each time.
  const profilerIframe = getProfilerIframe(doc);
  // Wait for the settings to render.
  await TestUtils.waitForCondition(
    () =>
      profilerIframe.contentDocument &&
      profilerIframe.contentDocument.querySelector(selector)
  );

  return profilerIframe.contentDocument;
}

/**
 * Open the performance profiler dialog with a real client.
 */
async function openProfilerDialogWithRealClient(doc) {
  info("Click on the Profile Runtime button");
  const profileButton = doc.querySelector(".qa-profile-runtime-button");
  profileButton.click();

  info("Wait for the rendering of the profiler UI");
  const contentDocument = await waitForProfilerUiRendering(
    doc,
    ".perf-presets"
  );
  await getActiveButtonFromText(contentDocument, "Start recording");
  info("The profiler UI is rendered!");
  return contentDocument;
}

/**
 * Open the performance profiler settings. This assumes the profiler dialog is
 * already open by the previous function openProfilerDialog.
 */
async function openProfilerSettings(doc) {
  const profilerDocument = getProfilerIframe(doc).contentDocument;

  // Select the custom preset.
  const perfPresetsSelect = await getNearestInputFromText(
    profilerDocument,
    "Settings"
  );
  setReactFriendlyInputValue(perfPresetsSelect, "custom");

  // Click on "Edit Settings".
  const editSettingsLink = await getElementFromDocumentByText(
    profilerDocument,
    "Edit Settings"
  );
  editSettingsLink.click();

  info("Wait for the rendering of the profiler settings UI");
  const contentDocument = await waitForProfilerUiRendering(
    doc,
    ".perf-aboutprofiling-remote"
  );
  info("The profiler settings UI is rendered!");
  return contentDocument;
}

async function saveSettingsAndGoBack(doc) {
  const profilerDocument = getProfilerIframe(doc).contentDocument;

  const saveSettingsAndGoBackButton = await getActiveButtonFromText(
    profilerDocument,
    "Save settings"
  );
  saveSettingsAndGoBackButton.click();

  info("Wait for the rendering of the profiler UI");
  const contentDocument = await waitForProfilerUiRendering(
    doc,
    ".perf-presets"
  );
  await getActiveButtonFromText(contentDocument, "Start recording");
  info("The profiler UI is rendered!");
  return contentDocument;
}
