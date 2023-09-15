/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doUrlbarNewTabTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doUrlbarTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();
    await doEnter();
    await openPopup("y");

    await trigger();
    await assert();
  });
}

async function doHandoffTest({ trigger, assert }) {
  await doTest(async browser => {
    BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
    await BrowserTestUtils.browserStopped(browser, "about:newtab");
    await SpecialPowers.spawn(browser, [], function () {
      const searchInput = content.document.querySelector(".fake-editable");
      searchInput.click();
    });
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);

    await trigger();
    await assert();
  });
}

async function doUrlbarAddonpageTest({ trigger, assert }) {
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
    BrowserTestUtils.startLoadingURIString(browser, extensionURL);
    await onLoad;
    await openPopup("x");

    await trigger();
    await assert();
  });

  await extension.unload();
}
