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

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
      ["middlemouse.contentLoadURL", false],
      ["test.events.async.enabled", true],
    ],
  });
});

async function doTest() {
  function httpURL(filename) {
    const chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  function getScrollY(context) {
    return SpecialPowers.spawn(context, [], () => content.scrollY);
  }

  const pageUrl = httpURL("helper_test_autoscrolling_in_oop_frame.html");

  await BrowserTestUtils.withNewTab(pageUrl, async function(browser) {
    await promiseApzFlushedRepaintsInPopup(browser);

    const iframeContext = browser.browsingContext.children[0];
    await promiseApzFlushedRepaintsInPopup(iframeContext);

    const { screenX, screenY, viewId, presShellId } = await SpecialPowers.spawn(
      iframeContext,
      [],
      () => {
        const winUtils = SpecialPowers.getDOMWindowUtils(content);
        return {
          screenX: content.mozInnerScreenX * content.devicePixelRatio,
          screenY: content.mozInnerScreenY * content.devicePixelRatio,
          viewId: winUtils.getViewId(content.document.documentElement),
          presShellId: winUtils.getPresShellId(),
        };
      }
    );

    ok(
      iframeContext.startApzAutoscroll(
        screenX + 100,
        screenY + 50,
        viewId,
        presShellId
      ),
      "Started autscroll"
    );

    const scrollEventPromise = SpecialPowers.spawn(
      iframeContext,
      [],
      async () => {
        return new Promise(resolve => {
          content.addEventListener(
            "scroll",
            event => {
              dump("Got a scroll event in the iframe\n");
              resolve();
            },
            { once: true }
          );
        });
      }
    );

    // Send sequential mousemove events to cause autoscrolling.
    for (let i = 0; i < 10; i++) {
      await promiseNativeMouseEventWithAPZ({
        type: "mousemove",
        target: browser,
        offsetX: 100,
        offsetY: 50 + i * 10,
      });
    }

    // Flush APZ repaints and waits for MozAfterPaint to make sure the scroll has
    // been reflected on the main thread.
    const apzPromise = promiseApzFlushedRepaintsInPopup(browser);

    await Promise.all([apzPromise, scrollEventPromise]);

    const frameScrollY = await getScrollY(iframeContext);
    ok(frameScrollY > 0, "Autoscrolled the iframe");

    const rootScrollY = await getScrollY(browser);
    ok(rootScrollY == 0, "Didn't scroll the root document");

    iframeContext.stopApzAutoscroll(viewId, presShellId);
  });
}

add_task(async function test_autoscroll_in_oop_iframe() {
  await doTest();
});

add_task(async function test_autoscroll_in_oop_iframe_with_os_zoom() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.textScaleFactor", 200]] });
  await doTest();
});
