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

add_task(async function check_newSavedLogin_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let loginsSaved = 0;
  const triggerHandler = () => loginsSaved++;
  const newSavedLoginListener = ASRouterTriggerListeners.get("newSavedLogin");

  // Previously initialized by the Router
  newSavedLoginListener.uninit();

  // Initialise listener
  await newSavedLoginListener.init(triggerHandler);

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerNewSavedPassword(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
      await BrowserTestUtils.waitForCondition(
        () => loginsSaved !== 0,
        "Wait for the observer notification to run"
      );
      is(loginsSaved, 1, "should receive observer notification");
    }
  );

  // Uninitialise listener
  newSavedLoginListener.uninit();

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerNewSavedPasswordAfterUninit(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
      await new Promise(resolve => executeSoon(resolve));
      is(loginsSaved, 1, "shouldn't receive obs. notification after uninit");
    }
  );
});
