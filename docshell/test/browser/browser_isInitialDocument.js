/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tag every new WindowGlobalParent with an expando indicating whether or not
// they were an initial document when they were created for the duration of this
// test.
function wasInitialDocumentObserver(subject) {
  subject._test_wasInitialDocument = subject.isInitialDocument;
}
Services.obs.addObserver(wasInitialDocumentObserver, "window-global-created");
SimpleTest.registerCleanupFunction(function () {
  Services.obs.removeObserver(
    wasInitialDocumentObserver,
    "window-global-created"
  );
});

add_task(async function new_about_blank_tab() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    is(
      browser.browsingContext.currentWindowGlobal.isInitialDocument,
      false,
      "After loading an actual, final about:blank in the tab, the field is false"
    );
  });
});

add_task(async function iframe_initial_about_blank() {
  await BrowserTestUtils.withNewTab(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/document-builder.sjs?html=com",
    async browser => {
      info("Create an iframe without any explicit location");
      await SpecialPowers.spawn(browser, [], async () => {
        const iframe = content.document.createElement("iframe");
        // Add the iframe to the DOM tree in order to be able to have its browsingContext
        content.document.body.appendChild(iframe);
        const { browsingContext } = iframe;

        is(
          iframe.contentDocument.isInitialDocument,
          true,
          "The field is true on just-created iframes"
        );
        let beforeLoadPromise = SpecialPowers.spawnChrome(
          [browsingContext],
          bc => [
            bc.currentWindowGlobal.isInitialDocument,
            bc.currentWindowGlobal._test_wasInitialDocument,
          ]
        );

        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
        });
        is(
          iframe.contentDocument.isInitialDocument,
          false,
          "The field is false after having loaded the final about:blank document"
        );
        let afterLoadPromise = SpecialPowers.spawnChrome(
          [browsingContext],
          bc => [
            bc.currentWindowGlobal.isInitialDocument,
            bc.currentWindowGlobal._test_wasInitialDocument,
          ]
        );

        // Wait to await the parent process promises, so we can't miss the "load" event.
        let [beforeIsInitial, beforeWasInitial] = await beforeLoadPromise;
        is(beforeIsInitial, true, "before load is initial in parent");
        is(beforeWasInitial, true, "before load was initial in parent");
        let [afterIsInitial, afterWasInitial] = await afterLoadPromise;
        is(afterIsInitial, false, "after load is not initial in parent");
        is(afterWasInitial, true, "after load was initial in parent");
        iframe.remove();
      });

      info("Create an iframe with a cross origin location");
      const iframeBC = await SpecialPowers.spawn(browser, [], async () => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src =
            // eslint-disable-next-line @microsoft/sdl/no-insecure-url
            "http://example.org/document-builder.sjs?html=org-iframe";
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      });

      is(
        iframeBC.currentWindowGlobal.isInitialDocument,
        false,
        "The field is true after having loaded the final document"
      );
    }
  );
});

add_task(async function window_open() {
  async function testWindowOpen({ browser, args, isCrossOrigin, willLoad }) {
    info(`Open popup with ${JSON.stringify(args)}`);
    const onNewTab = BrowserTestUtils.waitForNewTab(
      gBrowser,
      args[0] || "about:blank"
    );
    await SpecialPowers.spawn(
      browser,
      [args, isCrossOrigin, willLoad],
      async (args, crossOrigin, willLoad) => {
        const win = content.window.open(...args);
        is(
          win.document.isInitialDocument,
          true,
          "The field is true right after calling window.open()"
        );
        let beforeLoadPromise = SpecialPowers.spawnChrome(
          [win.browsingContext],
          bc => [
            bc.currentWindowGlobal.isInitialDocument,
            bc.currentWindowGlobal._test_wasInitialDocument,
          ]
        );

        // In cross origin, it is harder to watch for new document load, and if
        // no argument is passed no load will happen.
        if (!crossOrigin && willLoad) {
          await new Promise(r =>
            win.addEventListener("load", r, { once: true })
          );
          is(
            win.document.isInitialDocument,
            false,
            "The field becomes false right after the popup document is loaded"
          );
        }

        // Perform the await after the load to avoid missing it.
        let [beforeIsInitial, beforeWasInitial] = await beforeLoadPromise;
        is(beforeIsInitial, true, "before load is initial in parent");
        is(beforeWasInitial, true, "before load was initial in parent");
      }
    );
    const newTab = await onNewTab;
    const windowGlobal =
      newTab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (willLoad) {
      is(
        windowGlobal.isInitialDocument,
        false,
        "The field is false in the parent process after having loaded the final document"
      );
    } else {
      is(
        windowGlobal.isInitialDocument,
        true,
        "The field remains true in the parent process as nothing will be loaded"
      );
    }
    BrowserTestUtils.removeTab(newTab);
  }

  await BrowserTestUtils.withNewTab(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/document-builder.sjs?html=com",
    async browser => {
      info("Use window.open() with cross-origin document");
      await testWindowOpen({
        browser,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        args: ["http://example.org/document-builder.sjs?html=org-popup"],
        isCrossOrigin: true,
        willLoad: true,
      });

      info("Use window.open() with same-origin document");
      await testWindowOpen({
        browser,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        args: ["http://example.com/document-builder.sjs?html=com-popup"],
        isCrossOrigin: false,
        willLoad: true,
      });

      info("Use window.open() with final about:blank document");
      await testWindowOpen({
        browser,
        args: ["about:blank"],
        isCrossOrigin: false,
        willLoad: true,
      });

      info("Use window.open() with no argument");
      await testWindowOpen({
        browser,
        args: [],
        isCrossOrigin: false,
        willLoad: false,
      });
    }
  );
});

add_task(async function document_open() {
  await BrowserTestUtils.withNewTab(
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/document-builder.sjs?html=com",
    async browser => {
      is(browser.browsingContext.currentWindowGlobal.isInitialDocument, false);
      await SpecialPowers.spawn(browser, [], async () => {
        const iframe = content.document.createElement("iframe");
        // Add the iframe to the DOM tree in order to be able to have its browsingContext
        content.document.body.appendChild(iframe);
        const { browsingContext } = iframe;

        // Check the state before the call in both parent and content.
        is(
          iframe.contentDocument.isInitialDocument,
          true,
          "Is an initial document before calling document.open"
        );
        let beforeOpenParentPromise = SpecialPowers.spawnChrome(
          [browsingContext],
          bc => [
            bc.currentWindowGlobal.isInitialDocument,
            bc.currentWindowGlobal._test_wasInitialDocument,
            bc.currentWindowGlobal.innerWindowId,
          ]
        );

        // Run the `document.open` call with reduced permissions.
        iframe.contentWindow.eval(`
          document.open();
          document.write("new document");
          document.close();
        `);

        is(
          iframe.contentDocument.isInitialDocument,
          false,
          "Is no longer an initial document after calling document.open"
        );
        let [afterIsInitial, afterWasInitial, afterID] =
          await SpecialPowers.spawnChrome([browsingContext], bc => [
            bc.currentWindowGlobal.isInitialDocument,
            bc.currentWindowGlobal._test_wasInitialDocument,
            bc.currentWindowGlobal.innerWindowId,
          ]);
        let [beforeIsInitial, beforeWasInitial, beforeID] =
          await beforeOpenParentPromise;
        is(beforeIsInitial, true, "Should be initial before in the parent");
        is(beforeWasInitial, true, "Was initial before in the parent");
        is(afterIsInitial, false, "Should not be initial after in the parent");
        is(afterWasInitial, true, "Was initial after in the parent");
        is(beforeID, afterID, "Should be the same WindowGlobalParent");
      });
    }
  );
});

add_task(async function windowless_browser() {
  info("Create a Windowless browser");
  const browser = Services.appShell.createWindowlessBrowser(false);
  const { browsingContext } = browser;
  is(
    browsingContext.currentWindowGlobal.isInitialDocument,
    true,
    "The field is true for a freshly created WindowlessBrowser"
  );
  is(
    browser.currentURI.spec,
    "about:blank",
    "The location is immediately set to about:blank"
  );

  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  browser.docShell.createAboutBlankDocumentViewer(principal, principal);
  is(
    browsingContext.currentWindowGlobal.isInitialDocument,
    false,
    "The field becomes false when creating an artificial blank document"
  );

  info("Load a final about:blank document in it");
  const onLocationChange = new Promise(resolve => {
    let wpl = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
      onLocationChange() {
        browsingContext.webProgress.removeProgressListener(
          wpl,
          Ci.nsIWebProgress.NOTIFY_ALL
        );
        resolve();
      },
    };
    browsingContext.webProgress.addProgressListener(
      wpl,
      Ci.nsIWebProgress.NOTIFY_ALL
    );
  });
  browser.loadURI(Services.io.newURI("about:blank"), {
    triggeringPrincipal: principal,
  });
  info("Wait for the location change");
  await onLocationChange;
  is(
    browsingContext.currentWindowGlobal.isInitialDocument,
    false,
    "The field is false after the location change event"
  );
  browser.close();
});
