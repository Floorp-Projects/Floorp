/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-real-usb.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-real-usb.js",
  this
);

// Test that USB runtimes appear from the sidebar.
// Documentation for real usb tests in /documentation/TESTS_REAL_DEVICES.md
add_task(async function() {
  if (!isAvailable()) {
    ok(true, "Real usb runtime test is not available");
    return;
  }

  const { document, tab } = await openAboutDebuggingWithADB();

  for (const { sidebarInfo } of await getExpectedRuntimeAll()) {
    const { deviceName, shortName } = sidebarInfo;
    await waitUntil(() => findSidebarItemByText(deviceName, document));
    const usbRuntimeSidebarItem = findSidebarItemByText(deviceName, document);
    ok(
      usbRuntimeSidebarItem.textContent.includes(shortName),
      "The device name and short name of the usb runtime are visible in sidebar item " +
        `[${usbRuntimeSidebarItem.textContent}]`
    );
  }

  await removeTab(tab);
});
