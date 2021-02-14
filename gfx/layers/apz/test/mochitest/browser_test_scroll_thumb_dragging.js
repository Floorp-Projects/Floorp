add_task(async function() {
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

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const scrollPromise = new Promise(resolve => {
      content.window.addEventListener("scroll", resolve, { once: true });
    });
    const dragFinisher = await content.wrappedJSObject.promiseVerticalScrollbarDrag(
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
