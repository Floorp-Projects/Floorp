// The tab closing code leaves an uncaught rejection. This test has been
// whitelisted until the issue is fixed.
if (!gMultiProcessBrowser) {
  Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);
  PromiseTestUtils.expectUncaughtRejection(/is no longer, usable/);
}

const kBaseURI = "http://mochi.test:8888/browser/dom/base/test/empty.html";
const kPluginJS = "chrome://mochitests/content/browser/dom/base/test/plugin.js";
var testURLs = [
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoop.html",
  "http://mochi.test:8888/browser/dom/base/test/file_audioLoopInIframe.html",
  "http://mochi.test:8888/browser/dom/base/test/file_pluginAudio.html",
  "http://mochi.test:8888/browser/dom/base/test/file_webaudioLoop.html",
];

// We want to ensure that while audio is being played back, a background tab is
// treated the same as a foreground tab as far as timeout throttling is concerned.
// So we use a 100,000 second minimum timeout value for background tabs.  This
// means that in case the test fails, it will time out in practice, but just for
// sanity the test condition ensures that the observed timeout delay falls in
// this range.
const kMinTimeoutBackground = 100 * 1000 * 1000;

const kDelay = 10;

Services.scriptloader.loadSubScript(kPluginJS, this);

async function runTest(url) {
  let currentTab = gBrowser.selectedTab;
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kBaseURI);
  let newBrowser = gBrowser.getBrowserForTab(newTab);

  // Wait for the UI to indicate that audio is being played back.
  let promise = BrowserTestUtils.waitForAttribute("soundplaying", newTab, "true");
  newBrowser.loadURI(url);
  await promise;

  // Put the tab in the background.
  await BrowserTestUtils.switchTab(gBrowser, currentTab);

  let timeout = await ContentTask.spawn(newBrowser, kDelay, function(delay) {
    return new Promise(resolve => {
      let before = new Date();
      content.window.setTimeout(function() {
        let after = new Date();
        resolve(after - before);
      }, delay);
    });
  });
  ok(timeout <= kMinTimeoutBackground, `Got the correct timeout (${timeout})`);

  // All done.
  await BrowserTestUtils.removeTab(newTab);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.min_background_timeout_value", kMinTimeoutBackground],
  ]});
});

add_task(async function test() {
  for (var url of testURLs) {
    await runTest(url);
  }
});
