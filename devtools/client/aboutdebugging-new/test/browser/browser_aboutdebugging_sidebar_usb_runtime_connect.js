/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MOCKS_ROOT = CHROME_URL_ROOT + "mocks/";

/* import-globals-from mocks/head-client-wrapper-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-client-wrapper-mock.js", this);
/* import-globals-from mocks/head-runtime-client-factory-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-runtime-client-factory-mock.js",
  this);
/* import-globals-from mocks/head-usb-runtimes-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-usb-runtimes-mock.js", this);

const RUNTIME_ID = "test-runtime-id";

const { RUNTIMES } = require("devtools/client/aboutdebugging-new/src/constants");

// Test that USB runtimes appear and disappear from the sidebar.
add_task(async function() {
  const usbRuntimesMock = createUsbRuntimesMock();
  const observerMock = addObserverMock(usbRuntimesMock);
  enableUsbRuntimesMock(usbRuntimesMock);

  // Create a basic mocked ClientWrapper that only defines a custom description.
  const mockUsbClient = createClientMock();
  mockUsbClient.getDeviceDescription = () => {
    return {
      name: "TestBrand",
      channel: "release",
      version: "1.0",
    };
  };

  // Mock the runtime-helper module to return mocked ClientWrappers for our test runtime
  // ids.
  const RuntimeClientFactoryMock = createRuntimeClientFactoryMock();
  enableRuntimeClientFactoryMock(RuntimeClientFactoryMock);
  RuntimeClientFactoryMock.createClientForRuntime = runtime => {
    let client = null;
    if (runtime.id === RUNTIMES.THIS_FIREFOX) {
      client = createThisFirefoxClientMock();
    } else if (runtime.id === RUNTIME_ID) {
      client = mockUsbClient;
    }
    return { client };
  };

  // Disable mocks when the test ends.
  registerCleanupFunction(() => {
    disableRuntimeClientFactoryMock();
    disableUsbRuntimesMock();
  });

  const { document, tab } = await openAboutDebugging();

  usbRuntimesMock.getUSBRuntimes = function() {
    return [{
      id: RUNTIME_ID,
      _socketPath: "test/path",
      deviceName: "test device name",
      shortName: "testshort",
    }];
  };
  observerMock.emit("runtime-list-updated");

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText("test device name", document));
  const usbRuntimeSidebarItem = findSidebarItemByText("test device name", document);
  const connectButton = usbRuntimeSidebarItem.querySelector(".js-connect-button");
  ok(connectButton, "Connect button is displayed for the USB runtime");

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  await waitUntil(() => usbRuntimeSidebarItem.querySelector(".js-connect-button"));

  info("Remove all USB runtimes");
  usbRuntimesMock.getUSBRuntimes = function() {
    return [];
  };
  observerMock.emit("runtime-list-updated");

  info("Wait until the USB sidebar item disappears");
  await waitUntil(() => !findSidebarItemByText("test device name", document));

  await removeTab(tab);
});
