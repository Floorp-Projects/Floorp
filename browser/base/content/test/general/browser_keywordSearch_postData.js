/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

var gTests = [
  {
    name: "normal search (search service)",
    testText: "test search",
    expectText: "test+search"
  },
  {
    name: "?-prefixed search (search service)",
    testText: "?   foo  ",
    expectText: "foo"
  }
];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  let searchObserver = function search_observer(aSubject, aTopic, aData) {
    let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
    info("Observer: " + aData + " for " + engine.name);

    if (aData != "engine-added")
      return;

    if (engine.name != "POST Search")
      return;

    Services.search.defaultEngine = engine;

    registerCleanupFunction(function () {
      Services.search.removeEngine(engine);
    });

    // ready to execute the tests!
    executeSoon(nextTest);
  };

  Services.obs.addObserver(searchObserver, "browser-search-engine-modified", false);

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);

    Services.obs.removeObserver(searchObserver, "browser-search-engine-modified");
  });

  Services.search.addEngine("http://test:80/browser/browser/base/content/test/general/POSTSearchEngine.xml",
                            null, null, false);
}

var gCurrTest;
function nextTest() {
  if (gTests.length) {
    gCurrTest = gTests.shift();
    doTest();
  } else {
    finish();
  }
}

function doTest() {
  info("Running test: " + gCurrTest.name);

  waitForLoad(function () {
    let loadedText = gBrowser.contentDocument.body.textContent;
    ok(loadedText, "search page loaded");
    let needle = "searchterms=" + gCurrTest.expectText;
    is(loadedText, needle, "The query POST data should be returned in the response");
    nextTest();
  });

  // Simulate a user entering search terms
  gURLBar.value = gCurrTest.testText;
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}


function waitForLoad(cb) {
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function listener() {
    if (browser.currentURI.spec == "about:blank")
      return;
    info("Page loaded: " + browser.currentURI.spec);
    browser.removeEventListener("load", listener, true);

    cb();
  }, true);
}
