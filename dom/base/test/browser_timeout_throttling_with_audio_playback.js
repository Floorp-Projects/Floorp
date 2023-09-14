// The tab closing code leaves an uncaught rejection. This test has been
// whitelisted until the issue is fixed.
if (!gMultiProcessBrowser) {
  const { PromiseTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/PromiseTestUtils.sys.mjs"
  );
  PromiseTestUtils.expectUncaughtRejection(/is no longer, usable/);
}

const kBaseURI = "http://mochi.test:8888/browser/dom/base/test/empty.html";
var testURLs = [
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html",
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoopInIframe.html",
  "http://mochi.test:8888/browser/dom/base/test/file_webaudio_startstop.html",
];

// We want to ensure that while audio is being played back, a background tab is
// treated the same as a foreground tab as far as timeout throttling is concerned.
// So we use a 100,000 second minimum timeout value for background tabs.  This
// means that in case the test fails, it will time out in practice, but just for
// sanity the test condition ensures that the observed timeout delay falls in
// this range.
const kMinTimeoutBackground = 100 * 1000 * 1000;

const kDelay = 10;

async function runTest(url) {
  let currentTab = gBrowser.selectedTab;
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kBaseURI);
  let newBrowser = gBrowser.getBrowserForTab(newTab);

  // Wait for the UI to indicate that audio is being played back.
  let promise = BrowserTestUtils.waitForAttribute("soundplaying", newTab);
  BrowserTestUtils.startLoadingURIString(newBrowser, url);
  await promise;

  // Put the tab in the background.
  await BrowserTestUtils.switchTab(gBrowser, currentTab);

  let timeout = await SpecialPowers.spawn(
    newBrowser,
    [kDelay],
    function (delay) {
      return new Promise(resolve => {
        let before = new Date();
        content.window.setTimeout(function () {
          let after = new Date();
          resolve(after - before);
        }, delay);
      });
    }
  );
  ok(timeout <= kMinTimeoutBackground, `Got the correct timeout (${timeout})`);

  // All done.
  BrowserTestUtils.removeTab(newTab);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.min_background_timeout_value", kMinTimeoutBackground]],
  });
});

add_task(async function test() {
  for (var url of testURLs) {
    await runTest(url);
  }
});
