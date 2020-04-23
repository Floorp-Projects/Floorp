/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const chrome_base = getRootDirectory(gTestPath);

/* import-globals-from contextmenu_common.js */
Services.scriptloader.loadSubScript(
  chrome_base + "contextmenu_common.js",
  this
);
const http_base = chrome_base.replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function test_view_image_works({ page, selector }) {
  let mainURL = http_base + page;
  let accel = AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey";
  let tests = {
    tab: {
      event: { [accel]: true },
      async loadedPromise() {
        return BrowserTestUtils.waitForNewTab(
          gBrowser,
          url => url.startsWith("blob"),
          true
        ).then(t => t.linkedBrowser);
      },
      cleanup(browser) {
        BrowserTestUtils.removeTab(gBrowser.getTabForBrowser(browser));
      },
    },
    window: {
      event: { shiftKey: true },
      async loadedPromise() {
        // Unfortunately we can't predict the URL so can't just pass that to waitForNewWindow
        let w = await BrowserTestUtils.waitForNewWindow();
        let browser = w.gBrowser.selectedBrowser;
        let getCx = () => browser.browsingContext;
        await TestUtils.waitForCondition(
          () =>
            getCx() && getCx().currentWindowGlobal.documentURI.schemeIs("blob")
        );
        await SpecialPowers.spawn(browser, [], () => {
          return ContentTaskUtils.waitForCondition(
            () => content.document.readyState == "complete"
          );
        });
        return browser;
      },
      async cleanup(browser) {
        return BrowserTestUtils.closeWindow(browser.ownerGlobal);
      },
    },
    self: {
      event: {},
      async loadedPromise() {
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          url => url.startsWith("blob:")
        );
        return gBrowser.selectedBrowser;
      },
      async cleanup() {},
    },
    // NOTE: If we ever add more tests here, add them above and not below `self`, as it replaces
    // the test document.
  };
  await BrowserTestUtils.withNewTab(mainURL, async browser => {
    await SpecialPowers.spawn(browser, [], () => {
      return ContentTaskUtils.waitForCondition(
        () => !content.document.documentElement.classList.contains("wait")
      );
    });
    for (let [testLabel, test] of Object.entries(tests)) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      is(
        contextMenu.state,
        "closed",
        `${testLabel} - checking if popup is closed`
      );
      let promisePopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      await BrowserTestUtils.synthesizeMouse(
        selector,
        2,
        2,
        { type: "contextmenu", button: 2 },
        browser
      );
      await promisePopupShown;
      info(`${testLabel} - Popup Shown`);
      let promisePopupHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      let browserPromise = test.loadedPromise();
      EventUtils.synthesizeMouseAtCenter(
        document.getElementById("context-viewimage"),
        test.event
      );
      await promisePopupHidden;

      let newBrowser = await browserPromise;
      await SpecialPowers.spawn(newBrowser, [testLabel], msgPrefix => {
        let img = content.document.querySelector("img");
        ok(
          img instanceof Ci.nsIImageLoadingContent,
          `${msgPrefix} - Image should have loaded content.`
        );
        const request = img.getRequest(
          Ci.nsIImageLoadingContent.CURRENT_REQUEST
        );
        ok(
          request.imageStatus & request.STATUS_LOAD_COMPLETE,
          `${msgPrefix} - Should have loaded image.`
        );
      });
      await test.cleanup(newBrowser);
    }
  });
}

/**
 * Verify that the 'view image' context menu in a new tab for a canvas works,
 * when opened in a new tab, a new window, or in the same tab.
 */
add_task(async function test_view_image_canvas_works() {
  await test_view_image_works({
    page: "subtst_contextmenu.html",
    selector: "#test-canvas",
  });
});

/**
 * Test for https://bugzilla.mozilla.org/show_bug.cgi?id=1625786
 */
add_task(async function test_view_image_revoked_cached_blob() {
  await test_view_image_works({
    page: "test_view_image_revoked_cached_blob.html",
    selector: "#second",
  });
});
