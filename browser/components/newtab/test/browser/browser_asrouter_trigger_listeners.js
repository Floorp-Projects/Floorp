ChromeUtils.defineModuleGetter(
  this,
  "ASRouterTriggerListeners",
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

async function openURLInWindow(window, url) {
  const { selectedBrowser } = window.gBrowser;
  await BrowserTestUtils.loadURI(selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(selectedBrowser, false, url);
}

add_task(async function check_openURL_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let urlVisitCount = 0;
  const triggerHandler = () => urlVisitCount++;
  const openURLListener = ASRouterTriggerListeners.get("openURL");

  // Previously initialized by the Router
  openURLListener.uninit();

  const normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Initialise listener
  await openURLListener.init(triggerHandler, ["example.com"]);

  await openURLInWindow(normalWindow, TEST_URL);
  await BrowserTestUtils.waitForCondition(
    () => urlVisitCount !== 0,
    "Wait for the location change listener to run"
  );
  is(urlVisitCount, 1, "should receive page visits from existing windows");

  await openURLInWindow(normalWindow, "http://www.example.com/abc");
  is(urlVisitCount, 1, "should not receive page visits for different domains");

  await openURLInWindow(privateWindow, TEST_URL);
  is(
    urlVisitCount,
    1,
    "should not receive page visits from existing private windows"
  );

  const secondNormalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(secondNormalWindow, TEST_URL);
  await BrowserTestUtils.waitForCondition(
    () => urlVisitCount === 2,
    "Wait for the location change listener to run"
  );
  is(urlVisitCount, 2, "should receive page visits from newly opened windows");

  const secondPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await openURLInWindow(secondPrivateWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should not receive page visits from newly opened private windows"
  );

  // Uninitialise listener
  openURLListener.uninit();

  await openURLInWindow(normalWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should now not receive page visits from existing windows"
  );

  const thirdNormalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(thirdNormalWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should now not receive page visits from newly opened windows"
  );

  // Cleanup
  const windows = [
    normalWindow,
    privateWindow,
    secondNormalWindow,
    secondPrivateWindow,
    thirdNormalWindow,
  ];
  await Promise.all(windows.map(win => BrowserTestUtils.closeWindow(win)));
});
