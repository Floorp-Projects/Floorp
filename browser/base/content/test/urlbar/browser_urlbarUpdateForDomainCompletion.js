"use strict";

/**
 * Disable keyword.enabled (so no keyword search), and check that when you type in
 * "example" and hit enter, the browser loads and the URL bar is updated accordingly.
 */
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["keyword.enabled", false]]});
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (browser) {
    gURLBar.value = "example";
    gURLBar.select();
    let loadPromise = BrowserTestUtils.browserLoaded(browser, false, url => url == "http://www.example.com/");
    EventUtils.sendKey("return");
    yield loadPromise;
    is(gURLBar.textValue, "www.example.com");
  });
});
