/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test asserts that the sidebar shows a message with a closing button describing
 * the status of the USB devices debugging.
 */
add_task(async function() {
  const { document, tab } = await openAboutDebugging();

  const usbStatusElement = document.querySelector(".js-sidebar-usb-status");
  ok(usbStatusElement, "Sidebar shows the USB status element");
  ok(usbStatusElement.textContent.includes("USB disabled"),
    "USB status element has the expected content");

  const button = document.querySelector(".js-sidebar-item .qa-message-button-close");
  button.click();

  // new search for element
  ok(document.querySelector(".js-sidebar-usb-status") === null,
    "USB status element is no longer displayed");

  await removeTab(tab);
});
