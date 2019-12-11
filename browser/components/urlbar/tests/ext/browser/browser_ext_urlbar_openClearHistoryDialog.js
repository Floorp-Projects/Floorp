/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.openClearHistoryDialog
// WebExtension Experiment API.

"use strict";

add_task(async function normalMode() {
  await checkExtension();
});

add_task(async function privateBrowsingMode() {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await checkExtension(/* isPrivate */ true);
  await BrowserTestUtils.closeWindow(privateWindow);
});

async function checkExtension(isPrivate = false) {
  let ext = await loadExtension(async () => {
    browser.test.onMessage.addListener(async () => {
      // Testing without user input. We expect the call to fail.
      await browser.test.assertRejects(
        browser.experiments.urlbar.openClearHistoryDialog(),
        "experiments.urlbar.openClearHistoryDialog may only be called from a user input handler",
        "browser.experiments.urlbar.openClearHistoryDialog should fail when called from outside a user input handler."
      );
      // Testing with user input.
      browser.test.withHandlingUserInput(() => {
        browser.experiments.urlbar.openClearHistoryDialog();
      });
    });
  });

  let dialogPromise = new Promise((resolve, reject) => {
    function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener(
          "load",
          function() {
            if (
              subj.document.documentURI ==
              "chrome://browser/content/sanitize.xhtml"
            ) {
              Services.ww.unregisterNotification(onOpen);
              Assert.ok(true, "Observed Clear Recent History window open");
              // On macOS the Clear Recent History dialog gets opened as an
              // app-modal window, so its opener is null.
              is(
                subj.opener,
                AppConstants.platform == "macosx" ? null : window,
                "openClearHistoryDialog opened a sanitizer window."
              );
              subj.close();
              resolve();
            }
          },
          { once: true }
        );
      }
    }
    Services.ww.registerNotification(onOpen);
    if (isPrivate) {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => {
        Services.ww.unregisterNotification(onOpen);
        reject("No window opened.");
      }, 500);
    }
  });

  ext.sendMessage("begin-test");

  if (isPrivate) {
    await dialogPromise.catch(() => {
      Assert.ok(true, "Promise should have timed out in PBM.");
    });
  } else {
    await dialogPromise;
    Assert.ok(true, "Clear history browser dialog was shown.");
  }

  await ext.unload();
}
