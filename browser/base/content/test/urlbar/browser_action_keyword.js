function* promise_first_result(inputText) {
  yield promiseAutocompleteResultPopup(inputText);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  return firstResult;
}

const TEST_URL = "http://mochi.test:8888/browser/browser/base/content/test/urlbar/print_postdata.sjs";

add_task(function* setup() {
  yield PlacesUtils.keywords.insert({ keyword: "get",
                                      url: TEST_URL + "?q=%s" });
  yield PlacesUtils.keywords.insert({ keyword: "post",
                                      url: TEST_URL,
                                      postData: "q=%s" });
  registerCleanupFunction(function* () {
    yield PlacesUtils.keywords.remove("get");
    yield PlacesUtils.keywords.remove("post");
    while (gBrowser.tabs.length > 1) {
      yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  });
});

add_task(function* get_keyword() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  let result = yield promise_first_result("get something");
  isnot(result, null, "Expect a keyword result");

  let types = new Set(result.getAttribute("type").split(/\s+/));
  Assert.ok(types.has("keyword"));
  is(result.getAttribute("actiontype"), "keyword", "Expect correct `actiontype` attribute");
  is(result.getAttribute("title"), "mochi.test:8888", "Expect correct title");

  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, PlacesUtils.mozActionURI("keyword",
                                        { url: TEST_URL + "?q=something",
                                          input: "get something"}),
     "Expect correct url");

  let titleHbox = result._titleText.parentNode.parentNode;
  ok(titleHbox.classList.contains("ac-title"), "Title hbox element sanity check");
  is_element_visible(titleHbox, "Title element should be visible");
  is(result._titleText.textContent, "mochi.test:8888: something",
     "Node should contain the name of the bookmark and query");

  let urlHbox = result._urlText.parentNode.parentNode;
  ok(urlHbox.classList.contains("ac-url"), "URL hbox element sanity check");
  is_element_hidden(urlHbox, "URL element should be hidden");

  let actionHbox = result._actionText.parentNode.parentNode;
  ok(actionHbox.classList.contains("ac-action"), "Action hbox element sanity check");
  is_element_visible(actionHbox, "Action element should be visible");
  is(result._actionText.textContent, "", "Action text should be empty");

  // Click on the result
  info("Normal click on result");
  let tabPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeMouseAtCenter(result, {});
  yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, TEST_URL + "?q=something",
     "Tab should have loaded from clicking on result");

  // Middle-click on the result
  info("Middle-click on result");
  result = yield promise_first_result("get somethingmore");
  isnot(result, null, "Expect a keyword result");
  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, PlacesUtils.mozActionURI("keyword",
                                        { url: TEST_URL + "?q=somethingmore",
                                          input: "get somethingmore" }),
     "Expect correct url");

  tabPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(result, {button: 1});
  let tabOpenEvent = yield tabPromise;
  let newTab = tabOpenEvent.target;
  yield BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  is(newTab.linkedBrowser.currentURI.spec,
     TEST_URL + "?q=somethingmore",
     "Tab should have loaded from middle-clicking on result");
});


add_task(function* post_keyword() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  let result = yield promise_first_result("post something");
  isnot(result, null, "Expect a keyword result");

  let types = new Set(result.getAttribute("type").split(/\s+/));
  Assert.ok(types.has("keyword"));
  is(result.getAttribute("actiontype"), "keyword", "Expect correct `actiontype` attribute");
  is(result.getAttribute("title"), "mochi.test:8888", "Expect correct title");

  is(result.getAttribute("url"),
     PlacesUtils.mozActionURI("keyword", { url: TEST_URL,
                                           input: "post something",
                                           "postData": "q=something" }),
     "Expect correct url");

  // Click on the result
  info("Normal click on result");
  let tabPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeMouseAtCenter(result, {});
  yield tabPromise;
  is(tab.linkedBrowser.currentURI.spec, TEST_URL,
     "Tab should have loaded from clicking on result");

  let postData = yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    return content.document.body.textContent;
  });
  is(postData, "q=something", "post data was submitted correctly");
});
