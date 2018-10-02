/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

var gTests = [
  {
    name: "normal search (search service)",
    testText: "test search",
    expectText: "test+search",
  },
  {
    name: "?-prefixed search (search service)",
    testText: "?   foo  ",
    expectText: "foo",
  },
];

add_task(async function setup() {
  let engineAddedPromise = TestUtils.topicObserved("browser-search-engine-modified", (subject, data) => {
    return data == "engine-added";
  });

  Services.search.addEngine("http://test:80/browser/browser/base/content/test/general/POSTSearchEngine.xml",
                            null, null, false);

  let [subject, data] = await engineAddedPromise;

  let engine = subject.QueryInterface(Ci.nsISearchEngine);
  info("Observer: " + data + " for " + engine.name);

  if (engine.name != "POST Search") {
    Assert.ok(false, "Wrong search engine added");
  }

  Services.search.defaultEngine = engine;

  registerCleanupFunction(function() {
    Services.search.removeEngine(engine);
  });
});

add_task(async function() {
  for (let test of gTests) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    let browser = tab.linkedBrowser;

    // Simulate a user entering search terms
    gURLBar.value = test.testText;
    gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter");

    await BrowserTestUtils.browserLoaded(browser, false, url => {
      return url != "about:blank";
    });

    info("Page loaded: " + browser.currentURI.spec);

    let textContent = await ContentTask.spawn(browser, null, async () => {
      return content.document.body.textContent;
    });

    Assert.ok(textContent, "search page loaded");
    let needle = "searchterms=" + test.expectText;
    Assert.equal(textContent, needle, "The query POST data should be returned in the response");

    BrowserTestUtils.removeTab(tab);
  }
});
