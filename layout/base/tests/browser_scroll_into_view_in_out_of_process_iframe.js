"use strict";

add_task(async () => {
  function httpURL(filename, host = "https://example.com/") {
    let root = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content/",
      host
    );
    return root + filename;
  }

  const fissionWindow = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });
  const url = httpURL(
    "test_scroll_into_view_in_oopif.html",
    "http://mochi.test:8888/"
  );
  const crossOriginIframeUrl = httpURL("scroll_into_view_in_child.html");

  try {
    await BrowserTestUtils.withNewTab(
      { gBrowser: fissionWindow.gBrowser, url },
      async browser => {
        await SpecialPowers.spawn(
          browser,
          [crossOriginIframeUrl],
          async iframeUrl => {
            const iframe = content.document.getElementById("iframe");
            iframe.setAttribute("src", iframeUrl);

            // Wait for a scroll event since scrollIntoView for cross origin documents is
            // asyncronously processed.
            const scroller = content.document.getElementById("scroller");
            await new Promise(resolve => {
              scroller.addEventListener("scroll", resolve, { once: true });
            });

            ok(
              scroller.scrollTop > 0,
              "scrollIntoView works in a cross origin iframe"
            );
          }
        );
      }
    );
  } finally {
    await BrowserTestUtils.closeWindow(fissionWindow);
  }
});
