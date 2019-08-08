/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "1337id";
const DEVICE_NAME = "Fancy Phone";
const SERVER_RUNTIME_NAME = "Mozilla Firefox";
const ADB_RUNTIME_NAME = "Firefox Preview";
const SERVER_VERSION = "v7.3.31";
const ADB_VERSION = "v1.3.37";

const FENIX_RELEASE_ICON_SRC =
  "chrome://devtools/skin/images/aboutdebugging-fenix.svg";
const FENIX_NIGHTLY_ICON_SRC =
  "chrome://devtools/skin/images/aboutdebugging-fenix-nightly.svg";

/**
 * Check that Fenix runtime information is correctly displayed in about:debugging.
 */
add_task(async function() {
  const mocks = new Mocks();
  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: DEVICE_NAME,
    isFenix: true,
    name: SERVER_RUNTIME_NAME,
    shortName: ADB_RUNTIME_NAME,
    versionName: ADB_VERSION,
    version: SERVER_VERSION,
  });

  // open a remote runtime page
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, ADB_RUNTIME_NAME, document);

  info("Check that the runtime information is displayed as expected");
  const runtimeInfo = document.querySelector(".qa-runtime-name");
  ok(runtimeInfo, "Runtime info for the Fenix runtime is displayed");
  const runtimeInfoText = runtimeInfo.textContent;

  ok(runtimeInfoText.includes(ADB_RUNTIME_NAME), "Name is the ADB name");
  ok(
    !runtimeInfoText.includes(SERVER_RUNTIME_NAME),
    "Name does not include the server name"
  );

  ok(runtimeInfoText.includes(ADB_VERSION), "Version contains the ADB version");
  ok(
    !runtimeInfoText.includes(SERVER_VERSION),
    "Version does not contain the server version"
  );

  const runtimeIcon = document.querySelector(".qa-runtime-icon");
  is(
    runtimeIcon.src,
    FENIX_RELEASE_ICON_SRC,
    "The runtime icon is the Fenix icon"
  );

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, document);

  await removeTab(tab);
});

/**
 * Check that Fenix runtime information is correctly displayed in about:devtools-toolbox.
 */
add_task(async function() {
  // We use a real local client combined with a mocked USB runtime to be able to open
  // about:devtools-toolbox on a real target.
  const clientWrapper = await createLocalClientWrapper();

  // Mock getDeviceDescription() to force the local client to return "nightly" as the
  // channel. Otherwise the value of the icon source would depend on the current channel.
  const deviceDescription = await clientWrapper.getDeviceDescription();
  clientWrapper.getDeviceDescription = function() {
    return Object.assign({}, deviceDescription, {
      channel: "nightly",
    });
  };

  const mocks = new Mocks();
  mocks.createUSBRuntime(RUNTIME_ID, {
    clientWrapper: clientWrapper,
    deviceName: DEVICE_NAME,
    isFenix: true,
    name: SERVER_RUNTIME_NAME,
    shortName: ADB_RUNTIME_NAME,
    versionName: ADB_VERSION,
    version: SERVER_VERSION,
  });

  // open a remote runtime page
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.emitUSBUpdate();
  info("Select the runtime page for the USB runtime");
  const onRequestSuccess = waitForRequestsSuccess(window.AboutDebugging.store);
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, ADB_RUNTIME_NAME, document);

  info(
    "Wait for requests to finish the USB runtime is backed by a real local client"
  );
  await onRequestSuccess;

  info("Wait for the about:debugging target to be available");
  await waitUntil(() => findDebugTargetByText("about:debugging", document));
  const { devtoolsDocument, devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window
  );

  const runtimeInfo = devtoolsDocument.querySelector(".qa-runtime-info");
  const runtimeInfoText = runtimeInfo.textContent;
  ok(
    runtimeInfoText.includes(ADB_RUNTIME_NAME),
    "Name is the ADB runtime name"
  );
  ok(runtimeInfoText.includes(ADB_VERSION), "Version is the ADB version");

  const runtimeIcon = devtoolsDocument.querySelector(".qa-runtime-icon");
  is(
    runtimeIcon.src,
    FENIX_NIGHTLY_ICON_SRC,
    "The runtime icon is the Fenix icon"
  );

  info("Wait for all pending requests to settle on the DebuggerClient");
  await clientWrapper.client.waitForRequestsToSettle();

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Remove USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, document);

  await removeTab(tab);
  await clientWrapper.close();
});

async function createLocalClientWrapper() {
  info("Create a local DebuggerClient");
  const { DebuggerServer } = require("devtools/server/debugger-server");
  const { DebuggerClient } = require("devtools/shared/client/debugger-client");
  const {
    ClientWrapper,
  } = require("devtools/client/aboutdebugging-new/src/modules/client-wrapper");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const client = new DebuggerClient(DebuggerServer.connectPipe());

  await client.connect();
  return new ClientWrapper(client);
}
