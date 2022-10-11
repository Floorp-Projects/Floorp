/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_new_window_resize() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://example.net"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    info("Opening popup.");
    let win = this.content.open(
      "https://example.net",
      "",
      "width=200,height=200"
    );

    await ContentTaskUtils.waitForEvent(win, "load");

    let { outerWidth: initialWidth, outerHeight: initialHeight } = win;
    let targetWidth = initialWidth + 100;
    let targetHeight = initialHeight + 100;

    let observedOurResizeEvent = false;
    let resizeListener = () => {
      let { outerWidth: currentWidth, outerHeight: currentHeight } = win;
      info(`Resize event for ${currentWidth}x${currentHeight}.`);
      if (currentWidth == targetWidth && currentHeight == targetHeight) {
        ok(!observedOurResizeEvent, "First time we receive our resize event.");
        observedOurResizeEvent = true;
      }
    };
    win.addEventListener("resize", resizeListener);
    win.resizeTo(targetWidth, targetHeight);

    await ContentTaskUtils.waitForCondition(
      () => observedOurResizeEvent,
      `Waiting for our resize event (${targetWidth}x${targetHeight}).`
    );

    info("Waiting for potentially incoming resize events.");
    for (let i = 0; i < 10; i++) {
      await new Promise(r => win.requestAnimationFrame(r));
    }
    win.removeEventListener("resize", resizeListener);
    info("Closing popup.");
    win.close();
  });

  await BrowserTestUtils.removeTab(tab);
});
