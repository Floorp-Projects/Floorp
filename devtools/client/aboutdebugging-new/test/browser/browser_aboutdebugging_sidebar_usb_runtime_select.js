/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_DEVICE_ID = "1234";
const RUNTIME_DEVICE_NAME = "A device";

// Test that we can select a runtime in the sidebar
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime(RUNTIME_DEVICE_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(RUNTIME_DEVICE_NAME, document));
  const sidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
  const connectButton = sidebarItem.querySelector(".qa-connect-button");
  ok(connectButton, "Connect button is displayed for the USB runtime");

  info(
    "Click on the connect button and wait until the sidebar displays a link"
  );
  connectButton.click();
  await waitUntil(() =>
    findSidebarItemLinkByText(RUNTIME_DEVICE_NAME, document)
  );

  info("Click on the runtime link");
  const link = findSidebarItemLinkByText(RUNTIME_DEVICE_NAME, document);
  link.click();
  is(
    document.location.hash,
    `#/runtime/${RUNTIME_DEVICE_ID}`,
    "Redirection to runtime page"
  );

  await removeTab(tab);
});
