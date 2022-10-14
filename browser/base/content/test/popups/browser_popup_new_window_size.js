/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_new_window_size() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    baseURL
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    info("Opening popup.");
    let requestedWidth = 200;
    let requestedHeight = 200;
    let win = this.content.open(
      "popup_size.html",
      "",
      `width=${requestedWidth},height=${requestedHeight}`
    );

    let loadPromise = ContentTaskUtils.waitForEvent(win, "load");

    let { innerWidth: preLoadWidth, innerHeight: preLoadHeight } = win;
    is(preLoadWidth, requestedWidth, "Width before load event.");
    is(preLoadHeight, requestedHeight, "Height before load event.");

    await loadPromise;

    let { innerWidth: postLoadWidth, innerHeight: postLoadHeight } = win;
    is(postLoadWidth, requestedWidth, "Width after load event.");
    is(postLoadHeight, requestedHeight, "Height after load event.");

    await ContentTaskUtils.waitForCondition(
      () =>
        win.innerWidth == requestedWidth && win.innerHeight == requestedHeight,
      "Waiting for window to become request size."
    );

    let { innerWidth: finalWidth, innerHeight: finalHeight } = win;
    is(finalWidth, requestedWidth, "Final width.");
    is(finalHeight, requestedHeight, "Final height.");

    await SpecialPowers.spawn(
      win,
      [{ requestedWidth, requestedHeight }],
      async input => {
        let { initialSize, loadSize } = this.content.wrappedJSObject;
        is(
          initialSize.width,
          input.requestedWidth,
          "Content width before load event."
        );
        is(
          initialSize.height,
          input.requestedHeight,
          "Content height before load event."
        );
        is(
          loadSize.width,
          input.requestedWidth,
          "Content width after load event."
        );
        is(
          loadSize.height,
          input.requestedHeight,
          "Content height after load event."
        );
        is(
          this.content.innerWidth,
          input.requestedWidth,
          "Content final width."
        );
        is(
          this.content.innerHeight,
          input.requestedHeight,
          "Content final height."
        );
      }
    );

    info("Closing popup.");
    win.close();
  });

  await BrowserTestUtils.removeTab(tab);
});
