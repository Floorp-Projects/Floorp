add_task(async function () {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const newWin = await BrowserTestUtils.openNewBrowserWindow();

  const pageUrl = httpURL("helper_scroll_thumb_dragging.html");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    newWin.gBrowser,
    pageUrl
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
    await content.wrappedJSObject.waitUntilApzStable();
  });

  // Send an explicit click event to make sure the new window accidentally
  // doesn't get an "enter-notify-event" on Linux during dragging, the event
  // forcibly cancels the dragging state.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // Creating an object in this content privilege so that the object
    // properties can be accessed in below
    // promiseNativeMouseEventWithAPZAndWaitForEvent function.
    const moveParams = content.window.eval(`({
      target: window,
      type: "mousemove",
      offsetX: 10,
      offsetY: 10
    })`);
    const clickParams = content.window.eval(`({
      target: window,
      type: "click",
      offsetX: 10,
      offsetY: 10
    })`);
    // Send a mouse move event first to make sure the "enter-notify-event"
    // happens.
    await content.wrappedJSObject.promiseNativeMouseEventWithAPZAndWaitForEvent(
      moveParams
    );
    await content.wrappedJSObject.promiseNativeMouseEventWithAPZAndWaitForEvent(
      clickParams
    );
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const scrollPromise = new Promise(resolve => {
      content.window.addEventListener("scroll", resolve, { once: true });
    });
    const dragFinisher =
      await content.wrappedJSObject.promiseVerticalScrollbarDrag(
        content.window,
        10,
        10
      );

    await scrollPromise;
    await dragFinisher();

    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    ok(
      content.window.scrollY < 100,
      "The root scrollable content shouldn't be scrolled too much"
    );
  });

  await BrowserTestUtils.closeWindow(newWin);
});
