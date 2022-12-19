/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - sap

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function sap_urlbar_newtab() {
  await doTest(async browser => {
    await openPopup("x");
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]);
  });
});

add_task(async function sap_urlbar() {
  await doTest(async browser => {
    await openPopup("x");
    await doEnter();

    await openPopup("y");
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "urlbar" }]);
  });
});

add_task(async function sap_handoff() {
  await doTest(async browser => {
    BrowserTestUtils.loadURI(browser, "about:newtab");
    await BrowserTestUtils.browserStopped(browser, "about:newtab");
    await SpecialPowers.spawn(browser, [], function() {
      const searchInput = content.document.querySelector(".fake-editable");
      searchInput.click();
    });
    EventUtils.synthesizeKey("x");
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "handoff" }]);
  });
});

add_task(async function sap_urlbar_addonpage() {
  const extensionData = {
    files: {
      "page.html": "<!DOCTYPE html>hello",
    },
  };
  const extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  const extensionURL = `moz-extension://${extension.uuid}/page.html`;

  await doTest(async browser => {
    const onLoad = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(browser, extensionURL);
    await onLoad;

    await openPopup("x");
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "urlbar_addonpage" }]);
  });

  await extension.unload();
});
