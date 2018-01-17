/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported runTests */
// This file is imported into the same scope as head.js.
/* import-globals-from head.js */

{
  const chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);

  let localeDir = new URL("locale/", gTestPath).href;
  let {file} = chromeRegistry.convertChromeURL(Services.io.newURI(localeDir)).QueryInterface(Ci.nsIFileURL);

  Components.manager.addBootstrappedManifestLocation(file);
  registerCleanupFunction(() => {
    Components.manager.removeBootstrappedManifestLocation(file);
  });
}

async function runTests(options) {
  function background(getTests) {
    let tabs;
    let tests;

    // Gets the current details of the page action, and returns a
    // promise that resolves to an object containing them.
    async function getDetails() {
      let [tab] = await browser.tabs.query({active: true, currentWindow: true});
      let tabId = tab.id;

      browser.test.log(`Get details: tab={id: ${tabId}, url: ${JSON.stringify(tab.url)}}`);

      return {
        title: await browser.pageAction.getTitle({tabId}),
        popup: await browser.pageAction.getPopup({tabId}),
        isShown: await browser.pageAction.isShown({tabId}),
      };
    }


    // Runs the next test in the `tests` array, checks the results,
    // and passes control back to the outer test scope.
    function nextTest() {
      let test = tests.shift();

      test(async expecting => {
        function finish() {
          // Check that the actual icon has the expected values, then
          // run the next test.
          browser.test.sendMessage("nextTest", expecting, tests.length);
        }

        // Check that the API returns the expected values, and then
        // run the next test.
        let details = await getDetails();
        if (expecting) {
          browser.test.assertEq(expecting.title, details.title,
                                "expected value from getTitle");

          browser.test.assertEq(expecting.popup, details.popup,
                                "expected value from getPopup");
        }

        browser.test.assertEq(!!expecting, details.isShown,
                              "expected value from isShown");

        finish();
      });
    }

    async function runTests() {
      tabs = [];
      tests = getTests(tabs);

      let resultTabs = await browser.tabs.query({active: true, currentWindow: true});

      tabs[0] = resultTabs[0].id;

      nextTest();
    }

    browser.test.onMessage.addListener((msg) => {
      if (msg == "runTests") {
        runTests();
      } else if (msg == "runNextTest") {
        nextTest();
      } else {
        browser.test.fail(`Unexpected message: ${msg}`);
      }
    });

    runTests();
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,

    files: options.files || {},

    background: `(${background})(${options.getTests})`,
  });

  let pageActionId;
  let currentWindow = window;
  let windows = [];

  function checkDetails(details) {
    let image = currentWindow.document.getElementById(pageActionId);
    if (details == null) {
      ok(image == null || image.getAttribute("disabled") == "true", "image is disabled");
    } else {
      ok(image, "image exists");

      is(getListStyleImage(image), details.icon, "icon URL is correct");

      let title = details.title || options.manifest.name;
      is(image.getAttribute("tooltiptext"), title, "image title is correct");
      is(image.getAttribute("aria-label"), title, "image aria-label is correct");
      // TODO: Popup URL.
    }
  }

  let testNewWindows = 1;

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", async (expecting, testsRemaining) => {
      if (!pageActionId) {
        pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(makeWidgetId(extension.id));
      }

      await promiseAnimationFrame(currentWindow);

      checkDetails(expecting);

      if (testsRemaining) {
        extension.sendMessage("runNextTest");
      } else if (testNewWindows) {
        testNewWindows--;

        BrowserTestUtils.openNewBrowserWindow().then(window => {
          windows.push(window);
          currentWindow = window;
          return focusWindow(window);
        }).then(() => {
          extension.sendMessage("runTests");
        });
      } else {
        resolve();
      }
    });
  });

  let reqLoc = Services.locale.getRequestedLocales();
  Services.locale.setRequestedLocales(["es-ES"]);

  await extension.startup();

  await awaitFinish;

  await extension.unload();

  Services.locale.setRequestedLocales(reqLoc);

  let node = document.getElementById(pageActionId);
  is(node, null, "pageAction image removed from document");

  currentWindow = null;
  for (let win of windows.splice(0)) {
    node = win.document.getElementById(pageActionId);
    is(node, null, "pageAction image removed from second document");

    await BrowserTestUtils.closeWindow(win);
  }
}
