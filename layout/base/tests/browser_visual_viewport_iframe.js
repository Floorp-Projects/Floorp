"use strict";

add_task(async () => {
  function httpURL(filename, host = "https://example.com/") {
    let root = getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content/",
      host
    );
    return root + filename;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["apz.allow_zooming", true],
      ["dom.meta-viewport.enabled", true],
      ["dom.visualviewport.enabled,", true],
    ],
  });

  const fissionWindow = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });
  const url = httpURL(
    "test_visual_viewport_in_oopif.html",
    "http://mochi.test:8888/"
  );
  const crossOriginIframeUrl = httpURL("visual_viewport_in_child.html");

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

            let { width, height } = await new Promise(resolve => {
              content.window.addEventListener("message", msg => {
                resolve(msg.data);
              });
            });

            is(
              width,
              300,
              "visualViewport.width shouldn't be affected in out-of-process iframes"
            );
            is(
              height,
              300,
              "visualViewport.height shouldn't be affected in out-of-process iframes"
            );
          }
        );
      }
    );
  } finally {
    await BrowserTestUtils.closeWindow(fissionWindow);
  }
});
