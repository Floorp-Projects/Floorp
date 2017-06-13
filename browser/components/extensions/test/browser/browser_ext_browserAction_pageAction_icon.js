/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Test that various combinations of icon details specs, for both paths
// and ImageData objects, result in the correct image being displayed in
// all display resolutions.
add_task(async function testDetailsObjects() {
  function background() {
    function getImageData(color) {
      let canvas = document.createElement("canvas");
      canvas.width = 2;
      canvas.height = 2;
      let canvasContext = canvas.getContext("2d");

      canvasContext.clearRect(0, 0, canvas.width, canvas.height);
      canvasContext.fillStyle = color;
      canvasContext.fillRect(0, 0, 1, 1);

      return {
        url: canvas.toDataURL("image/png"),
        imageData: canvasContext.getImageData(0, 0, canvas.width, canvas.height),
      };
    }

    let imageData = {
      red: getImageData("red"),
      green: getImageData("green"),
    };

    /* eslint-disable comma-dangle, indent */
    let iconDetails = [
      // Only paths.
      {details: {"path": "a.png"},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png")}},
      {details: {"path": "/a.png"},
        resolutions: {
          "1": browser.runtime.getURL("a.png"),
          "2": browser.runtime.getURL("a.png")}},
      {details: {"path": {"19": "a.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png")}},
      {details: {"path": {"38": "a.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png")}},
      {details: {"path": {"19": "a.png", "38": "a-x2.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a-x2.png")}},

      // Test that CSS strings are escaped properly.
      {details: {"path": 'a.png#" \\'},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png#%22%20%5C"),
          "2": browser.runtime.getURL("data/a.png#%22%20%5C")}},

      // Only ImageData objects.
      {details: {"imageData": imageData.red.imageData},
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url}},
      {details: {"imageData": {"19": imageData.red.imageData}},
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url}},
      {details: {"imageData": {"38": imageData.red.imageData}},
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url}},
      {details: {"imageData": {
          "19": imageData.red.imageData,
          "38": imageData.green.imageData}},
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.green.url}},

      // Mixed path and imageData objects.
      //
      // The behavior is currently undefined if both |path| and
      // |imageData| specify icons of the same size.
      {details: {
          "path": {"19": "a.png"},
          "imageData": {"38": imageData.red.imageData}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": imageData.red.url}},
      {details: {
          "path": {"38": "a.png"},
          "imageData": {"19": imageData.red.imageData}},
        resolutions: {
          "1": imageData.red.url,
          "2": browser.runtime.getURL("data/a.png")}},

      // A path or ImageData object by itself is treated as a 19px icon.
      {details: {
          "path": "a.png",
          "imageData": {"38": imageData.red.imageData}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": imageData.red.url}},
      {details: {
          "path": {"38": "a.png"},
          "imageData": imageData.red.imageData},
        resolutions: {
          "1": imageData.red.url,
          "2": browser.runtime.getURL("data/a.png")}},

      // Various resolutions
      {details: {"path": {"18": "a.png", "36": "a-x2.png"}},
        legacy: true,
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a-x2.png")}},
      {details: {"path": {"16": "a.png", "30": "a-x2.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a-x2.png")}},
      {details: {"path": {"16": "16.png", "100": "100.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/16.png"),
          "2": browser.runtime.getURL("data/100.png")}},
      {details: {"path": {"2": "2.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/2.png"),
          "2": browser.runtime.getURL("data/2.png")}},
      {details: {"path": {
        "16": "16.svg",
        "18": "18.svg"}},
        resolutions: {
          "1": browser.runtime.getURL("data/16.svg"),
          "2": browser.runtime.getURL("data/18.svg")}},
      {details: {"path": {
        "6": "6.png",
        "18": "18.png",
        "36": "36.png",
        "48": "48.png",
        "128": "128.png"}},
        legacy: true,
        resolutions: {
          "1": browser.runtime.getURL("data/18.png"),
          "2": browser.runtime.getURL("data/36.png")},
        menuResolutions: {
          "1": browser.runtime.getURL("data/36.png"),
          "2": browser.runtime.getURL("data/128.png")}},
      {details: {"path": {
        "16": "16.png",
        "18": "18.png",
        "32": "32.png",
        "48": "48.png",
        "64": "64.png",
        "128": "128.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/16.png"),
          "2": browser.runtime.getURL("data/32.png")},
        menuResolutions: {
          "1": browser.runtime.getURL("data/32.png"),
          "2": browser.runtime.getURL("data/64.png")}},
      {details: {"path": {
        "18": "18.png",
        "32": "32.png",
        "48": "48.png",
        "128": "128.png"}},
        resolutions: {
          "1": browser.runtime.getURL("data/32.png"),
          "2": browser.runtime.getURL("data/32.png")}},
    ];

    // Allow serializing ImageData objects for logging.
    ImageData.prototype.toJSON = () => "<ImageData>";

    let tabId;

    browser.test.onMessage.addListener((msg, test) => {
      if (msg != "setIcon") {
        browser.test.fail("expecting 'setIcon' message");
      }

      let details = iconDetails[test.index];

      let detailString = JSON.stringify(details);
      browser.test.log(`Setting browerAction/pageAction to ${detailString} expecting URLs ${JSON.stringify(details.resolutions)}`);

      Promise.all([
        browser.browserAction.setIcon(Object.assign({tabId}, details.details)),
        browser.pageAction.setIcon(Object.assign({tabId}, details.details)),
      ]).then(() => {
        browser.test.sendMessage("iconSet");
      });
    });

    // Generate a list of tests and resolutions to send back to the test
    // context.
    //
    // This process is a bit convoluted, because the outer test context needs
    // to handle checking the button nodes and changing the screen resolution,
    // but it can't pass us icon definitions with ImageData objects. This
    // shouldn't be a problem, since structured clones should handle ImageData
    // objects without issue. Unfortunately, |cloneInto| implements a slightly
    // different algorithm than we use in web APIs, and does not handle them
    // correctly.
    let tests = [];
    for (let [idx, icon] of iconDetails.entries()) {
      tests.push({
        index: idx,
        legacy: !!icon.legacy,
        menuResolutions: icon.menuResolutions,
        resolutions: icon.resolutions,
      });
    }

    // Sort by resolution, so we don't needlessly switch back and forth
    // between each test.
    tests.sort(test => test.resolution);

    browser.tabs.query({active: true, currentWindow: true}, tabs => {
      tabId = tabs[0].id;
      browser.pageAction.show(tabId).then(() => {
        browser.test.sendMessage("ready", tests);
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {},
      "page_action": {},
      "background": {
        "page": "data/background.html",
      }
    },

    files: {
      "data/background.html": `<script src="background.js"></script>`,
      "data/background.js": background,

      "data/16.svg": imageBuffer,
      "data/18.svg": imageBuffer,

      "data/16.png": imageBuffer,
      "data/18.png": imageBuffer,
      "data/32.png": imageBuffer,
      "data/36.png": imageBuffer,
      "data/48.png": imageBuffer,
      "data/64.png": imageBuffer,
      "data/128.png": imageBuffer,

      "a.png": imageBuffer,
      "data/2.png": imageBuffer,
      "data/100.png": imageBuffer,
      "data/a.png": imageBuffer,
      "data/a-x2.png": imageBuffer,
    },
  });

  const RESOLUTION_PREF = "layout.css.devPixelsPerPx";

  await extension.startup();

  let pageActionId = `${makeWidgetId(extension.id)}-page-action`;
  let browserActionWidget = getBrowserActionWidget(extension);

  let tests = await extension.awaitMessage("ready");
  for (let test of tests) {
    extension.sendMessage("setIcon", test);
    await extension.awaitMessage("iconSet");

    await promiseAnimationFrame();

    let browserActionButton = browserActionWidget.forWindow(window).node;
    let pageActionImage = document.getElementById(pageActionId);


    // Test icon sizes in the toolbar/urlbar.
    for (let resolution of Object.keys(test.resolutions)) {
      await SpecialPowers.pushPrefEnv({set: [[RESOLUTION_PREF, resolution]]});

      is(window.devicePixelRatio, +resolution, "window has the required resolution");

      let imageURL = test.resolutions[resolution];
      is(getListStyleImage(browserActionButton), imageURL, `browser action has the correct image at ${resolution}x resolution`);
      is(getListStyleImage(pageActionImage), imageURL, `page action has the correct image at ${resolution}x resolution`);

      let isLegacy = browserActionButton.classList.contains("toolbarbutton-legacy-addon");
      is(isLegacy, test.legacy, "Legacy class should be present?");

      await SpecialPowers.popPrefEnv();
    }

    if (!test.menuResolutions) {
      continue;
    }


    if (!gPhotonStructure) {
      // Test icon sizes in the menu panel.
      CustomizableUI.addWidgetToArea(browserActionWidget.id,
                                     CustomizableUI.AREA_PANEL);

      await showBrowserAction(extension);
      browserActionButton = browserActionWidget.forWindow(window).node;

      for (let resolution of Object.keys(test.menuResolutions)) {
        await SpecialPowers.pushPrefEnv({set: [[RESOLUTION_PREF, resolution]]});

        is(window.devicePixelRatio, +resolution, "window has the required resolution");

        let imageURL = test.menuResolutions[resolution];
        is(getListStyleImage(browserActionButton), imageURL, `browser action has the correct menu image at ${resolution}x resolution`);

        await SpecialPowers.popPrefEnv();
      }

      await closeBrowserAction(extension);

      CustomizableUI.addWidgetToArea(browserActionWidget.id,
                                     CustomizableUI.AREA_NAVBAR);
    }
  }

  await extension.unload();
});
