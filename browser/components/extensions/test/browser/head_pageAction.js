/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported runTests */
// This file is imported into the same scope as head.js.
/* import-globals-from head.js */

{
  // At the moment extension language negotiation is tied to Firefox language
  // negotiation result. That means that to test an extension in `es-ES`, we need
  // to mock `es-ES` being available in Firefox and then request it.
  //
  // In the future, we should provide some way for tests to decouple their
  // language selection from that of Firefox.
  const avLocales = Services.locale.getAvailableLocales();

  Services.locale.setAvailableLocales(["en-US", "es-ES"]);
  registerCleanupFunction(() => {
    Services.locale.setAvailableLocales(avLocales);
  });
}

async function runTests(options) {
  function background(getTests) {
    let tests;

    // Gets the current details of the page action, and returns a
    // promise that resolves to an object containing them.
    async function getDetails(tabId) {
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
        let [tab] = await browser.tabs.query({active: true, currentWindow: true});
        let {id: tabId, windowId, url} = tab;

        browser.test.log(`Get details: tab={id: ${tabId}, url: ${url}}`);

        // Check that the API returns the expected values, and then
        // run the next test.
        let details = await getDetails(tabId);
        if (expecting) {
          browser.test.assertEq(expecting.title, details.title,
                                "expected value from getTitle");

          browser.test.assertEq(expecting.popup, details.popup,
                                "expected value from getPopup");
        }

        browser.test.assertEq(!!expecting, details.isShown,
                              "expected value from isShown");

        // Check that the actual icon has the expected values, then
        // run the next test.
        browser.test.sendMessage("nextTest", expecting, windowId, tests.length);
      });
    }

    async function runTests() {
      let tabs = [];
      let windows = [];
      tests = getTests(tabs, windows);

      let resultTabs = await browser.tabs.query({active: true, currentWindow: true});

      tabs[0] = resultTabs[0].id;
      windows[0] = resultTabs[0].windowId;

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

  function checkDetails(details, windowId) {
    let {document} = Services.wm.getOuterWindowWithId(windowId);
    let image = document.getElementById(pageActionId);
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
    extension.onMessage("nextTest", async (expecting, windowId, testsRemaining) => {
      if (!pageActionId) {
        pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(makeWidgetId(extension.id));
      }

      await promiseAnimationFrame(currentWindow);

      checkDetails(expecting, windowId);

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
