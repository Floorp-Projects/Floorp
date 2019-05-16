/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Check extension debug setting on USB runtime.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  info("Set initial state for test");
  await pushPref("devtools.debugger.remote-enabled", false);

  // Setup USB devices mocks
  const mocks = new Mocks();
  const usbRuntime = mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });
  const extension = { name: "Test extension name", debuggable: true };
  usbRuntime.listAddons = () => [extension];

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  mocks.emitUSBUpdate();

  info("Check the status of extension debug setting checkbox and inspect buttons");
  await connectToRuntime("Fancy Phone", document);
  await selectRuntime("Fancy Phone", "Lorem ipsum", document);
  ok(!document.querySelector(".qa-extension-debug-checkbox"),
     "Extension debug setting checkbox should not exist");
  const buttons = document.querySelectorAll(".qa-debug-target-inspect-button");
  ok([...buttons].every(b => !b.disabled), "All inspect buttons should be enabled");

  await removeTab(tab);
});
