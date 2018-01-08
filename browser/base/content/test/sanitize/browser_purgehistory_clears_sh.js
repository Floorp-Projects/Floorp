/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const url = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.org") + "dummy_page.html";

add_task(async function purgeHistoryTest() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
  }, async function purgeHistoryTestInner(browser) {
    let backButton = browser.ownerDocument.getElementById("Browser:Back");
    let forwardButton = browser.ownerDocument.getElementById("Browser:Forward");

    ok(!browser.webNavigation.canGoBack,
       "Initial value for webNavigation.canGoBack");
    ok(!browser.webNavigation.canGoForward,
       "Initial value for webNavigation.canGoBack");
    ok(backButton.hasAttribute("disabled"), "Back button is disabled");
    ok(forwardButton.hasAttribute("disabled"), "Forward button is disabled");

    await ContentTask.spawn(browser, null, async function() {
      let startHistory = content.history.length;
      content.history.pushState({}, "");
      content.history.pushState({}, "");
      content.history.back();
      let newHistory = content.history.length;
      Assert.equal(startHistory, 1, "Initial SHistory size");
      Assert.equal(newHistory, 3, "New SHistory size");
    });

    ok(browser.webNavigation.canGoBack, true,
       "New value for webNavigation.canGoBack");
    ok(browser.webNavigation.canGoForward, true,
       "New value for webNavigation.canGoForward");
    ok(!backButton.hasAttribute("disabled"), "Back button was enabled");
    ok(!forwardButton.hasAttribute("disabled"), "Forward button was enabled");


    let tmp = {};
    Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tmp);

    let {Sanitizer} = tmp;
    let sanitizer = new Sanitizer();

    await sanitizer.sanitize(["history"]);

    await ContentTask.spawn(browser, null, async function() {
      Assert.equal(content.history.length, 1, "SHistory correctly cleared");
    });

    ok(!browser.webNavigation.canGoBack,
       "webNavigation.canGoBack correctly cleared");
    ok(!browser.webNavigation.canGoForward,
       "webNavigation.canGoForward correctly cleared");
    ok(backButton.hasAttribute("disabled"), "Back button was disabled");
    ok(forwardButton.hasAttribute("disabled"), "Forward button was disabled");
  });
});
