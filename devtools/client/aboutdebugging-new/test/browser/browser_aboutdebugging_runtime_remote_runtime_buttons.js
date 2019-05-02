/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const USB_RUNTIME_ID = "1337id";
const USB_DEVICE_NAME = "Fancy Phone";
const USB_APP_NAME = "Lorem ipsum";

/**
 * Test that remote runtimes show action buttons that are hidden for 'This Firefox'.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  mocks.createUSBRuntime(USB_RUNTIME_ID, {
    deviceName: USB_DEVICE_NAME,
    name: USB_APP_NAME,
  });

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Checking This Firefox");
  ok(!document.querySelector(".qa-connection-prompt-toggle-button"),
    "This Firefox does not contain the connection prompt button");
  ok(!document.querySelector(".qa-profile-runtime-button"),
    "This Firefox does not contain the profile runtime button");
  ok(!document.querySelector(".qa-runtime-info__action"),
    "This Firefox does not contain the disconnect button");

  info("Checking a USB runtime");
  mocks.emitUSBUpdate();
  await connectToRuntime(USB_DEVICE_NAME, document);
  await selectRuntime(USB_DEVICE_NAME, USB_APP_NAME, document);
  ok(!!document.querySelector(".qa-connection-prompt-toggle-button"),
    "Runtime contains the connection prompt button");
  ok(!!document.querySelector(".qa-profile-runtime-button"),
    "Remote runtime contains the profile runtime button");
  ok(!!document.querySelector(".qa-runtime-info__action"),
    "Runtime contains the disconnect button");

  await removeTab(tab);
});
