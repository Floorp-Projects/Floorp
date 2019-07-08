/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_NAME = "test runtime name";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_SHORT_NAME = "test short name";

const CONNECTION_TIMING_OUT_DELAY = 1000;
const CONNECTION_CANCEL_DELAY = 2000;

// Test following connection state tests.
// * Connect button label and state will change during connecting.
// * Show error message if connection failed.
// * Show warninng if connection has been taken time.
add_task(async function() {
  await setupPreferences();

  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime(RUNTIME_ID, {
    name: RUNTIME_NAME,
    deviceName: RUNTIME_DEVICE_NAME,
    shortName: RUNTIME_SHORT_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(RUNTIME_DEVICE_NAME, document));
  const usbRuntimeSidebarItem = findSidebarItemByText(
    RUNTIME_DEVICE_NAME,
    document
  );
  const connectButton = usbRuntimeSidebarItem.querySelector(
    ".qa-connect-button"
  );

  info("Simulate to happen connection error");
  mocks.runtimeClientFactoryMock.createClientForRuntime = async runtime => {
    throw new Error("Dummy connection error");
  };

  info(
    "Check whether the error message displayed after clicking connect button"
  );
  connectButton.click();
  await waitUntil(() => document.querySelector(".qa-connection-error"));
  ok(true, "Error message displays when connection failed");

  info("Simulate to wait for the connection prompt on remote runtime");
  let resumeConnection;
  const resumeConnectionPromise = new Promise(r => {
    resumeConnection = r;
  });
  mocks.runtimeClientFactoryMock.createClientForRuntime = async runtime => {
    await resumeConnectionPromise;
    return mocks._clients[runtime.type][runtime.id];
  };

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  info("Check whether a warning of connection not responding displays");
  await waitUntil(() =>
    document.querySelector(".qa-connection-not-responding")
  );
  ok(
    document.querySelector(".qa-connection-not-responding"),
    "A warning of connection not responding displays"
  );
  ok(connectButton.disabled, "Connect button is disabled");
  ok(
    connectButton.textContent.startsWith("Connecting"),
    "Label of the connect button changes"
  );
  ok(
    !document.querySelector(".qa-connection-error"),
    "Error message disappears"
  );

  info(
    "Unblock the connection and check the message and connect button disappear"
  );
  resumeConnection();
  await waitUntil(
    () => !usbRuntimeSidebarItem.querySelector(".qa-connect-button")
  );
  ok(!document.querySelector(".qa-connection-error"), "Error disappears");
  ok(
    !document.querySelector(".qa-connection-not-responding"),
    "Warning disappears"
  );

  info("Remove a USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  await removeTab(tab);
});

// Test whether the status of all will be reverted after a certain period of time during
// waiting connection.
add_task(async function() {
  await setupPreferences();

  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime(RUNTIME_ID, {
    name: RUNTIME_NAME,
    deviceName: RUNTIME_DEVICE_NAME,
    shortName: RUNTIME_SHORT_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(RUNTIME_DEVICE_NAME, document));
  const usbRuntimeSidebarItem = findSidebarItemByText(
    RUNTIME_DEVICE_NAME,
    document
  );
  const connectButton = usbRuntimeSidebarItem.querySelector(
    ".qa-connect-button"
  );

  let resumeConnection;
  const resumeConnectionPromise = new Promise(r => {
    resumeConnection = r;
  });
  mocks.runtimeClientFactoryMock.createClientForRuntime = async runtime => {
    await resumeConnectionPromise;
    return mocks._clients[runtime.type][runtime.id];
  };

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  await waitUntil(() =>
    document.querySelector(".qa-connection-not-responding")
  );
  info("Check whether the all status will be reverted");
  await waitUntil(
    () => !document.querySelector(".qa-connection-not-responding")
  );
  ok(
    document.querySelector(".qa-connection-timeout"),
    "Connection timeout message displays"
  );
  ok(!connectButton.disabled, "Connect button is enabled");
  is(
    connectButton.textContent,
    "Connect",
    "Label of the connect button reverted"
  );
  ok(
    !document.querySelector(".qa-connection-error"),
    "Error message disappears"
  );

  info("Check whether the timeout message disappears");
  resumeConnection();
  await waitUntil(() => !document.querySelector(".qa-connection-timeout"));

  info("Remove a USB runtime");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item disappears");
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  await removeTab(tab);
});

async function setupPreferences() {
  await pushPref(
    "devtools.aboutdebugging.test-connection-timing-out-delay",
    CONNECTION_TIMING_OUT_DELAY
  );
  await pushPref(
    "devtools.aboutdebugging.test-connection-cancel-delay",
    CONNECTION_CANCEL_DELAY
  );
}
