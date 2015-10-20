/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Test that various combinations of icon details specs, for both paths
// and ImageData objects, result in the correct image being displayed in
// all display resolutions.
add_task(function* testDetailsObjects() {
  function background() {
    function getImageData(color) {
      var canvas = document.createElement("canvas");
      canvas.width = 2;
      canvas.height = 2;
      var canvasContext = canvas.getContext("2d");

      canvasContext.clearRect(0, 0, canvas.width, canvas.height);
      canvasContext.fillStyle = color;
      canvasContext.fillRect(0, 0, 1, 1);

      return {
        url: canvas.toDataURL("image/png"),
        imageData: canvasContext.getImageData(0, 0, canvas.width, canvas.height),
      };
    }

    var imageData = {
      red: getImageData("red"),
      green: getImageData("green"),
    };

    var iconDetails = [
      // Only paths.
      { details: { "path": "a.png" },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png"), } },
      { details: { "path": "/a.png" },
        resolutions: {
          "1": browser.runtime.getURL("a.png"),
          "2": browser.runtime.getURL("a.png"), } },
      { details: { "path": { "19": "a.png" } },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png"), } },
      { details: { "path": { "38": "a.png" } },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a.png"), } },
      { details: { "path": { "19": "a.png", "38": "a-x2.png" } },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": browser.runtime.getURL("data/a-x2.png"), } },

      // Only ImageData objects.
      { details: { "imageData": imageData.red.imageData },
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url, } },
      { details: { "imageData": { "19": imageData.red.imageData } },
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url, } },
      { details: { "imageData": { "38": imageData.red.imageData } },
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.red.url, } },
      { details: { "imageData": {
          "19": imageData.red.imageData,
          "38": imageData.green.imageData } },
        resolutions: {
          "1": imageData.red.url,
          "2": imageData.green.url, } },

      // Mixed path and imageData objects.
      //
      // The behavior is currently undefined if both |path| and
      // |imageData| specify icons of the same size.
      { details: {
          "path": { "19": "a.png" },
          "imageData": { "38": imageData.red.imageData } },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": imageData.red.url, } },
      { details: {
          "path": { "38": "a.png" },
          "imageData": { "19": imageData.red.imageData } },
        resolutions: {
          "1": imageData.red.url,
          "2": browser.runtime.getURL("data/a.png"), } },

      // A path or ImageData object by itself is treated as a 19px icon.
      { details: {
          "path": "a.png",
          "imageData": { "38": imageData.red.imageData } },
        resolutions: {
          "1": browser.runtime.getURL("data/a.png"),
          "2": imageData.red.url, } },
      { details: {
          "path": { "38": "a.png" },
          "imageData": imageData.red.imageData, },
        resolutions: {
          "1": imageData.red.url,
          "2": browser.runtime.getURL("data/a.png"), } },
    ];

    // Allow serializing ImageData objects for logging.
    ImageData.prototype.toJSON = () => "<ImageData>";

    var tabId;

    browser.test.onMessage.addListener((msg, test) => {
      if (msg != "setIcon") {
        browser.test.fail("expecting 'setIcon' message");
      }

      var details = iconDetails[test.index];
      var expectedURL = details.resolutions[test.resolution];

      var detailString = JSON.stringify(details);
      browser.test.log(`Setting browerAction/pageAction to ${detailString} expecting URL ${expectedURL}`)

      browser.browserAction.setIcon(Object.assign({tabId}, details.details));
      browser.pageAction.setIcon(Object.assign({tabId}, details.details));

      browser.test.sendMessage("imageURL", expectedURL);
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
    var tests = [];
    for (var [idx, icon] of iconDetails.entries()) {
      for (var res of Object.keys(icon.resolutions)) {
        tests.push({ index: idx, resolution: Number(res) });
      }
    }

    // Sort by resolution, so we don't needlessly switch back and forth
    // between each test.
    tests.sort(test => test.resolution);

    browser.tabs.query({ active: true, currentWindow: true }, tabs => {
      tabId = tabs[0].id;
      browser.pageAction.show(tabId);

      browser.test.sendMessage("ready", tests);
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
    },
  });

  const RESOLUTION_PREF = "layout.css.devPixelsPerPx";
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref(RESOLUTION_PREF);
  });

  let browserActionId = makeWidgetId(extension.id) + "-browser-action";
  let pageActionId = makeWidgetId(extension.id) + "-page-action";

  let [, tests] = yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  for (let test of tests) {
    SpecialPowers.setCharPref(RESOLUTION_PREF, String(test.resolution));
    is(window.devicePixelRatio, test.resolution, "window has the required resolution");

    extension.sendMessage("setIcon", test);

    let imageURL = yield extension.awaitMessage("imageURL");

    let browserActionButton = document.getElementById(browserActionId);
    is(browserActionButton.getAttribute("image"), imageURL, "browser action has the correct image");

    let pageActionImage = document.getElementById(pageActionId);
    is(pageActionImage.src, imageURL, "page action has the correct image");
  }

  yield extension.unload();
});

// Test that default icon details in the manifest.json file are handled
// correctly.
add_task(function *testDefaultDetails() {
  // TODO: Test localized variants.
  let icons = [
    "foo/bar.png",
    "/foo/bar.png",
    { "19": "foo/bar.png" },
    { "38": "foo/bar.png" },
    { "19": "foo/bar.png", "38": "baz/quux.png" },
  ];

  let expectedURL = new RegExp(String.raw`^moz-extension://[^/]+/foo/bar\.png$`);

  for (let icon of icons) {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        "browser_action": { "default_icon": icon },
        "page_action": { "default_icon": icon },
      },

      background: function () {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          var tabId = tabs[0].id;

          browser.pageAction.show(tabId);
          browser.test.sendMessage("ready");
        });
      }
    });

    yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

    let browserActionId = makeWidgetId(extension.id) + "-browser-action";
    let pageActionId = makeWidgetId(extension.id) + "-page-action";

    let browserActionButton = document.getElementById(browserActionId);
    let image = browserActionButton.getAttribute("image");

    ok(expectedURL.test(image), `browser action image ${image} matches ${expectedURL}`);

    let pageActionImage = document.getElementById(pageActionId);
    image = pageActionImage.src;

    ok(expectedURL.test(image), `page action image ${image} matches ${expectedURL}`);

    yield extension.unload();

    let node = document.getElementById(pageActionId);
    is(node, undefined, "pageAction image removed from document");
  }
});


// Check that attempts to load a privileged URL as an icon image fail.
add_task(function* testSecureURLsDenied() {

  // Test URLs passed to setIcon.

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {},
      "page_action": {},
    },

    background: function () {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        var tabId = tabs[0].id;

        var urls = ["chrome://browser/content/browser.xul",
                    "javascript:true"];

        for (var url of urls) {
          for (var api of ["pageAction", "browserAction"]) {
            try {
              browser[api].setIcon({tabId, path: url});

              browser.test.fail(`Load of '${url}' succeeded. Expected failure.`);
              browser.test.notifyFail("setIcon security tests");
              return;
            } catch (e) {
              // We can't actually inspect the error here, since the
              // error object belongs to the privileged scope of the API,
              // rather than to the extension scope that calls into it.
              // Just assume it's the expected security error, for now.
              browser.test.succeed(`Load of '${url}' failed. Expected failure.`);
            }
          }
        }

        browser.test.notifyPass("setIcon security tests");
      });
    },
  });

  yield extension.startup();

  yield extension.awaitFinish();
  yield extension.unload();


  // Test URLs included in the manifest.

  let urls = ["chrome://browser/content/browser.xul",
              "javascript:true"];

  let matchURLForbidden = url => ({
    message: new RegExp(`Loading extension.*Access to.*'${url}' denied`),
  });

  let messages = [matchURLForbidden(urls[0]),
                  matchURLForbidden(urls[1]),
                  matchURLForbidden(urls[0]),
                  matchURLForbidden(urls[1])];

  let waitForConsole = new Promise(resolve => {
    // Not necessary in browser-chrome tests, but monitorConsole gripes
    // if we don't call it.
    SimpleTest.waitForExplicitFinish();

    SimpleTest.monitorConsole(resolve, messages);
  });

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_icon": {
          "19": urls[0],
          "38": urls[1],
        },
      },
      "page_action": {
        "default_icon": {
          "19": urls[0],
          "38": urls[1],
        },
      },
    },
  });

  yield extension.startup();
  yield extension.unload();

  SimpleTest.endMonitorConsole();
  yield waitForConsole;
});
