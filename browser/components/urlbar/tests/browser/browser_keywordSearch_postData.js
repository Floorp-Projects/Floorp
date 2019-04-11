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

  const url = "http://test:80/browser/browser/components/urlbar/tests/browser/POSTSearchEngine.xml";
  await Services.search.addEngine(url, null, false);

  let [subject, data] = await engineAddedPromise;

  let engine = subject.QueryInterface(Ci.nsISearchEngine);
  info("Observer: " + data + " for " + engine.name);

  if (engine.name != "POST Search") {
    Assert.ok(false, "Wrong search engine added");
  }

  await Services.search.setDefault(engine);

  registerCleanupFunction(async function() {
    await Services.search.removeEngine(engine);
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
