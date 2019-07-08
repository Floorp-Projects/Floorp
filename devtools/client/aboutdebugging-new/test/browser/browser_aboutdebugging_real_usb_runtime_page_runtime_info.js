/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-real-usb.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-real-usb.js",
  this
);

// Test that runtime info of USB runtime appears on main pane.
// Documentation for real usb tests in /documentation/TESTS_REAL_DEVICES.md
add_task(async function() {
  if (!isAvailable()) {
    ok(true, "Real usb runtime test is not available");
    return;
  }

  const { document, tab } = await openAboutDebuggingWithADB();

  const expectedRuntime = await getExpectedRuntime();
  const { runtimeDetails, sidebarInfo } = expectedRuntime;

  info("Connect a USB runtime");
  await Promise.race([
    connectToRuntime(sidebarInfo.deviceName, document),
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    new Promise(resolve =>
      setTimeout(() => {
        ok(
          false,
          "Failed to connect, did you disable the connection prompt for this runtime?"
        );
        resolve();
      }, 5000)
    ),
    /* eslint-enable mozilla/no-arbitrary-setTimeout */
  ]);

  info("Select a USB runtime");
  await selectRuntime(
    sidebarInfo.deviceName,
    runtimeDetails.info.name,
    document
  );

  info("Check that runtime info is properly displayed");
  const runtimeInfo = document.querySelector(".qa-runtime-name");
  ok(runtimeInfo, "Runtime info is displayed");
  const runtimeInfoText = runtimeInfo.textContent;
  ok(
    runtimeInfoText.includes(runtimeDetails.info.name),
    "Runtime info shows the correct runtime name: " + runtimeInfoText
  );
  ok(
    runtimeInfoText.includes(runtimeDetails.info.version),
    "Runtime info shows the correct version number: " + runtimeInfoText
  );

  await removeTab(tab);
});
