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
// So we use a 10ms minimum timeout value for foreground tabs and a 100,000 second
// minimum timeout value for background tabs.  This means that in case the test
// fails, it will time out in practice, but just for sanity the test condition
// ensures that the observed timeout delay falls in this range.
const kMinTimeoutForeground = 10;
const kMinTimeoutBackground = 100 * 1000 * 1000;

Services.scriptloader.loadSubScript(kPluginJS, this);

function* runTest(url) {
  let currentTab = gBrowser.selectedTab;
  let newTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kBaseURI);
  let newBrowser = gBrowser.getBrowserForTab(newTab);

  // Wait for the UI to indicate that audio is being played back.
  let promise = BrowserTestUtils.waitForAttribute("soundplaying", newTab, "true");
  newBrowser.loadURI(url);
  yield promise;

  // Put the tab in the background.
  yield BrowserTestUtils.switchTab(gBrowser, currentTab);

  let timeout = yield ContentTask.spawn(newBrowser, {}, function() {
    return new Promise(resolve => {
      let before = new Date();
      content.window.setTimeout(function() {
        let after = new Date();
        // Sometimes due to rounding errors, we may get a result of 9ms here, so
        // let's round up by 1 to protect against such intermittent failures.
        resolve(after - before + 1);
      }, 0);
    });
  });
  ok(timeout >= kMinTimeoutForeground &&
     timeout <  kMinTimeoutBackground, `Got the correct timeout (${timeout})`);

  // All done.
  yield BrowserTestUtils.removeTab(newTab);
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({"set": [
    ["dom.min_timeout_value", kMinTimeoutForeground],
    ["dom.min_background_timeout_value", kMinTimeoutBackground],
  ]});
});

add_task(function* test() {
  for (var url of testURLs) {
    yield runTest(url);
  }
});
