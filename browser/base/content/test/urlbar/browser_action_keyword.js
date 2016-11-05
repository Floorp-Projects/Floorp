/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gOnSearchComplete = null;

function* promise_first_result(inputText) {
  yield promiseAutocompleteResultPopup(inputText);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  return firstResult;
}


add_task(function*() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  let tabs = [tab];
  registerCleanupFunction(function* () {
    for (let tab of tabs)
      gBrowser.removeTab(tab);
    yield PlacesUtils.bookmarks.remove(bm);
  });

  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/?q=%s",
                                                title: "test" });
  yield PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });

  let result = yield promise_first_result("keyword something");
  isnot(result, null, "Expect a keyword result");

  let types = new Set(result.getAttribute("type").split(/\s+/));
  Assert.ok(types.has("keyword"));
  is(result.getAttribute("actiontype"), "keyword", "Expect correct `actiontype` attribute");
  is(result.getAttribute("title"), "example.com", "Expect correct title");

  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, PlacesUtils.mozActionURI("keyword", {url: "http://example.com/?q=something", input: "keyword something"}), "Expect correct url");

  let titleHbox = result._titleText.parentNode.parentNode;
  ok(titleHbox.classList.contains("ac-title"), "Title hbox element sanity check");
  is_element_visible(titleHbox, "Title element should be visible");
  is(result._titleText.textContent, "example.com: something", "Node should contain the name of the bookmark and query");

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
  is(tab.linkedBrowser.currentURI.spec, "http://example.com/?q=something", "Tab should have loaded from clicking on result");

  // Middle-click on the result
  info("Middle-click on result");
  result = yield promise_first_result("keyword somethingmore");
  isnot(result, null, "Expect a keyword result");
  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, PlacesUtils.mozActionURI("keyword", {url: "http://example.com/?q=somethingmore", input: "keyword somethingmore"}), "Expect correct url");

  tabPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(result, {button: 1});
  let tabOpenEvent = yield tabPromise;
  let newTab = tabOpenEvent.target;
  tabs.push(newTab);
  yield BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  is(newTab.linkedBrowser.currentURI.spec, "http://example.com/?q=somethingmore", "Tab should have loaded from middle-clicking on result");
});
