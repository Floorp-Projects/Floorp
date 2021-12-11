/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const USB_RUNTIME_ID = "1337id";
const USB_DEVICE_NAME = "Fancy Phone";
const USB_APP_NAME = "Lorem ipsum";

const DEFAULT_PAGE = "#/runtime/this-firefox";

/**
 * Check if the disconnect button disconnects the remote runtime
 * and redirects to the default page.
 */
add_task(async function() {
  // Create a real local client and use it as the remote USB client for this
  // test.
  const clientWrapper = await createLocalClientWrapper();

  // enable USB devices mocks
  const mocks = new Mocks();
  mocks.createUSBRuntime(USB_RUNTIME_ID, {
    clientWrapper,
    deviceName: USB_DEVICE_NAME,
    name: USB_APP_NAME,
  });

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.emitUSBUpdate();

  const onRequestSuccess = waitForRequestsSuccess(window.AboutDebugging.store);
  await connectToRuntime(USB_DEVICE_NAME, document);
  await selectRuntime(USB_DEVICE_NAME, USB_APP_NAME, document);
  await onRequestSuccess;

  const disconnectRemoteRuntimeButton = document.querySelector(
    ".qa-runtime-info__action"
  );

  info("Check whether disconnect remote runtime button exists");
  ok(!!disconnectRemoteRuntimeButton, "Runtime contains the disconnect button");

  info("Click on the disconnect button");
  disconnectRemoteRuntimeButton.click();

  info("Wait until the runtime is disconnected");
  await waitUntil(() => document.querySelector(".qa-connect-button"));

  is(
    document.location.hash,
    DEFAULT_PAGE,
    "Redirection to the default page (this-firefox)"
  );

  info("Wait until the Runtime name is displayed");
  await waitUntil(() => {
    const runtimeInfo = document.querySelector(".qa-runtime-name");
    return runtimeInfo && runtimeInfo.textContent.includes("Firefox");
  });

  await removeTab(tab);
});
