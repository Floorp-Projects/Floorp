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
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const url = httpURL("helper_position_sticky_flicker.html");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const { rect, scrollbarWidth } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      const sticky = content.document.getElementById("sticky");

      // Get the area in the screen coords where the position:sticky element is.
      let stickyRect = sticky.getBoundingClientRect();
      stickyRect.x += content.window.mozInnerScreenX;
      stickyRect.y += content.window.mozInnerScreenY;

      // generate some DIVs to make the page complex enough.
      for (let i = 1; i <= 120000; i++) {
        const div = content.document.createElement("div");
        div.innerText = `${i}`;
        content.document.body.appendChild(div);
      }

      await content.wrappedJSObject.promiseApzFlushedRepaints();
      await content.wrappedJSObject.waitUntilApzStable();

      let w = {},
        h = {};
      SpecialPowers.DOMWindowUtils.getScrollbarSizes(
        content.document.documentElement,
        w,
        h
      );

      // Reduce the scrollbar width from the sticky area.
      stickyRect.width -= w.value;
      return {
        rect: stickyRect,
        scrollbarWidth: w.value,
      };
    }
  );

  // Take a snapshot where the position:sticky element is initially painted.
  const reference = await getSnapshot(rect);

  let mouseX = window.innerWidth - scrollbarWidth / 2;
  let mouseY = tab.linkedBrowser.getBoundingClientRect().y + 5;

  // Scroll fast to cause checkerboarding multiple times.
  const dragFinisher = await promiseNativeMouseDrag(
    window,
    mouseX,
    mouseY,
    0,
    window.innerHeight,
    100
  );

  // On debug builds there seems to be no chance that the content process gets
  // painted during above promiseNativeMouseDrag call, wait two frames to make
  // sure it happens so that this test is likely able to fail without proper
  // fix.
  if (AppConstants.DEBUG) {
    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      await content.wrappedJSObject.promiseFrame(content.window);
      await content.wrappedJSObject.promiseFrame(content.window);
    });
  }

  // Take a snapshot again where the position:sticky element should be painted.
  const snapshot = await getSnapshot(rect);

  await dragFinisher();

  is(
    snapshot,
    reference,
    "The position:sticky element should stay at the " +
      "same place after scrolling on heavy load"
  );

  BrowserTestUtils.removeTab(tab);
});
