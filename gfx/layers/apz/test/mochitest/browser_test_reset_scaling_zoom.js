add_task(async function setup_pref() {
  let isWindows = navigator.platform.indexOf("Win") == 0;
  await SpecialPowers.pushPrefEnv({
    set: [["apz.test.fails_with_native_injection", isWindows]],
  });
});

add_task(async function test_main() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const pageUrl = httpURL("helper_test_reset_scaling_zoom.html");

  await BrowserTestUtils.withNewTab(pageUrl, async function (browser) {
    let getResolution = function () {
      return content.window.SpecialPowers.getDOMWindowUtils(
        content.window
      ).getResolution();
    };

    let doZoomIn = async function () {
      await content.window.wrappedJSObject.doZoomIn();
    };

    let resolution = await ContentTask.spawn(browser, null, getResolution);
    is(resolution, 1.0, "Initial page resolution should be 1.0");

    await ContentTask.spawn(browser, null, doZoomIn);
    resolution = await ContentTask.spawn(browser, null, getResolution);
    isnot(resolution, 1.0, "Expected resolution to be bigger than 1.0");

    document.getElementById("cmd_fullZoomReset").doCommand();
    // Spin the event loop once just to make sure the message gets through
    await new Promise(resolve => setTimeout(resolve, 0));

    resolution = await ContentTask.spawn(browser, null, getResolution);
    is(resolution, 1.0, "Expected resolution to be reset to 1.0");
  });
});
