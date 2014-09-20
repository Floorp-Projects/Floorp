/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gOnSearchComplete = null;

function* promise_first_result(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield promiseSearchComplete();
  // On Linux, the popup may or may not be open at this stage. So we need
  // additional checks to ensure we wait long enough.
  yield promisePopupShown(gURLBar.popup);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  return firstResult;
}


add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete"))
    return;

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  let tabs = [tab];
  registerCleanupFunction(() => {
    for (let tab of tabs)
      gBrowser.removeTab(tab);
    PlacesUtils.bookmarks.removeItem(itemId);
  });

  yield promiseTabLoadEvent(tab);

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://example.com/?q=%s"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");

  let result = yield promise_first_result("keyword something");
  isnot(result, null, "Expect a keyword result");

  is(result.getAttribute("type"), "action keyword", "Expect correct  `type` attribute");
  is(result.getAttribute("actiontype"), "keyword", "Expect correct `actiontype` attribute");
  is(result.getAttribute("title"), "test", "Expect correct title");

  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, makeActionURI("keyword", {url: "http://example.com/?q=something", input: "keyword something"}).spec, "Expect correct url");

  is_element_visible(result._title, "Title element should be visible");
  is(result._title.childNodes.length, 1, "Title element should have 1 child");
  is(result._title.childNodes[0].nodeName, "#text", "That child should be a text node");
  is(result._title.childNodes[0].data, "test", "Node should contain the name of the bookmark");

  is_element_visible(result._extra, "Extra element should be visible");
  is(result._extra.childNodes.length, 1, "Title element should have 1 child");
  is(result._extra.childNodes[0].nodeName, "span", "That child should be a span node");
  let span = result._extra.childNodes[0];
  is(span.childNodes.length, 1, "span element should have 1 child");
  is(span.childNodes[0].nodeName, "#text", "That child should be a text node");
  is(span.childNodes[0].data, "something", "Node should contain the query for the keyword");

  is_element_hidden(result._url, "URL element should be hidden");

  // Click on the result
  info("Normal click on result");
  let tabPromise = promiseTabLoadEvent(tab);
  EventUtils.synthesizeMouseAtCenter(result, {});
  let loadEvent = yield tabPromise;
  is(loadEvent.target.location.href, "http://example.com/?q=something", "Tab should have loaded from clicking on result");

  // Middle-click on the result
  info("Middle-click on result");
  result = yield promise_first_result("keyword somethingmore");
  isnot(result, null, "Expect a keyword result");
  // We need to make a real URI out of this to ensure it's normalised for
  // comparison.
  uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, makeActionURI("keyword", {url: "http://example.com/?q=somethingmore", input: "keyword somethingmore"}).spec, "Expect correct url");

  tabPromise = promiseWaitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(result, {button: 1});
  let tabOpenEvent = yield tabPromise;
  let newTab = tabOpenEvent.target;
  tabs.push(newTab);
  loadEvent = yield promiseTabLoadEvent(newTab);
  is(loadEvent.target.location.href, "http://example.com/?q=somethingmore", "Tab should have loaded from middle-clicking on result");
});
