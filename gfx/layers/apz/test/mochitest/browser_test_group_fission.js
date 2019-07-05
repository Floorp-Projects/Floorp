add_task(async function test_main() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  var utils = SpecialPowers.getDOMWindowUtils(window);
  var isWebRender = utils.layerManagerType == "WebRender";

  // Each of these URLs will get opened in a new top-level browser window that
  // is fission-enabled.
  var test_urls = [
    httpURL("helper_fission_basic.html"),
    // add additional tests here
  ];
  if (isWebRender) {
    test_urls = test_urls.concat([
      httpURL("helper_fission_transforms.html"),
      httpURL("helper_fission_scroll_oopif.html"),
      // add additional WebRender-specific tests here
    ]);
  }

  let fissionWindow = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  // We import the JSM here so that we can install functions on the class
  // below.
  const { FissionTestHelperParent } = ChromeUtils.import(
    getRootDirectory(gTestPath) + "FissionTestHelperParent.jsm"
  );
  FissionTestHelperParent.SimpleTest = SimpleTest;

  ChromeUtils.registerWindowActor("FissionTestHelper", {
    parent: {
      moduleURI: getRootDirectory(gTestPath) + "FissionTestHelperParent.jsm",
    },
    child: {
      moduleURI: getRootDirectory(gTestPath) + "FissionTestHelperChild.jsm",
      events: {
        DOMWindowCreated: {},
      },
    },
    allFrames: true,
  });

  try {
    for (var url of test_urls) {
      dump(`Starting test ${url}\n`);

      // Load the test URL and tell it to get started, and wait until it reports
      // completion.
      await BrowserTestUtils.withNewTab(
        { gBrowser: fissionWindow.gBrowser, url },
        async browser => {
          let tabActor = browser.browsingContext.currentWindowGlobal.getActor(
            "FissionTestHelper"
          );
          let donePromise = tabActor.getTestCompletePromise();
          tabActor.startTest();
          await donePromise;
        }
      );

      dump(`Finished test ${url}\n`);
    }
  } finally {
    // Delete stuff we added to FissionTestHelperParent, beacuse the object will
    // outlive this test, and leaving stuff on it may leak the things reachable
    // from it.
    delete FissionTestHelperParent.SimpleTest;
    // Teardown
    ChromeUtils.unregisterWindowActor("FissionTestHelper");
    await BrowserTestUtils.closeWindow(fissionWindow);
  }
});
