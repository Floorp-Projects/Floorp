/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

add_task(async () => {
  // Use pan gesture events for Mac.
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set a relatively shorter timeout value.
      ["apz.content_response_timeout", 100],
    ],
  });

  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );
  // Load a content having an APZ-aware listener causing 500ms busy state and
  // a scroll event listener changing the background color of an element.
  // The reason why we change the background color in a scroll listener rather
  // than setting up a Promise resolved in a scroll event handler and waiting
  // for the Promise is SpecialPowers.spawn doesn't allow it (bug 1743857).
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URL_ROOT + "helper_content_response_timeout.html"
  );

  let scrollPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "scroll"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
    await content.wrappedJSObject.waitUntilApzStable();
  });

  // Note that below function uses `WaitForObserver` version of sending a
  // pan-start event function so that the notification can be sent in the parent
  // process, thus we can get the notification even if the content process is
  // busy.
  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    0,
    NativePanHandler.delta,
    NativePanHandler.beginPhase
  );

  await new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, 200);
  });

  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    0,
    NativePanHandler.delta,
    NativePanHandler.updatePhase
  );
  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    0,
    0,
    NativePanHandler.endPhase
  );

  await scrollPromise;
  ok(true, "We got at least one scroll event");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
