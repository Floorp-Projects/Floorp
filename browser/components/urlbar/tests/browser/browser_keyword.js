/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests that keywords are displayed and handled correctly.
 */

async function promise_first_result(inputText) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: inputText,
  });

  return UrlbarTestUtils.getDetailsOfResultAt(window, 0);
}

function assertURL(result, expectedUrl, keyword, input, postData) {
  Assert.equal(result.url, expectedUrl, "Should have the correct URL");
  if (postData) {
    Assert.equal(
      NetUtil.readInputStreamToString(
        result.postData,
        result.postData.available()
      ),
      postData,
      "Should have the correct postData"
    );
  }
}

const TEST_URL = `${TEST_BASE_URL}print_postdata.sjs`;

add_setup(async function () {
  await PlacesUtils.keywords.insert({
    keyword: "get",
    url: TEST_URL + "?q=%s",
  });
  await PlacesUtils.keywords.insert({
    keyword: "post",
    url: TEST_URL,
    postData: "q=%s",
  });
  await PlacesUtils.keywords.insert({
    keyword: "question?",
    url: TEST_URL + "?q2=%s",
  });
  await PlacesUtils.keywords.insert({
    keyword: "?question",
    url: TEST_URL + "?q3=%s",
  });
  // Avoid fetching search suggestions.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });
  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
    setAsDefault: true,
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.keywords.remove("get");
    await PlacesUtils.keywords.remove("post");
    await PlacesUtils.keywords.remove("question?");
    await PlacesUtils.keywords.remove("?question");
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  });
});

add_task(async function test_display_keyword_without_query() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  // Test a keyword that also has blank spaces to ensure they are ignored as well.
  let result = await promise_first_result("get  ");

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result"
  );
  Assert.equal(
    result.displayed.title,
    "example.com/browser/browser/components/urlbar/tests/browser/print_postdata.sjs?q=",
    "Node should contain the url of the bookmark"
  );
  let [action] = await document.l10n.formatValues([
    { id: "urlbar-result-action-visit" },
  ]);
  Assert.equal(result.displayed.action, action, "Should have visit indicated");
});

add_task(async function test_keyword_using_get() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  let result = await promise_first_result("get something");

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result"
  );
  Assert.equal(
    result.displayed.title,
    "example.com: something",
    "Node should contain the name of the bookmark and query"
  );
  Assert.ok(!result.displayed.action, "Should have an empty action");

  assertURL(result, TEST_URL + "?q=something", "get", "get something");

  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

  // Click on the result
  info("Normal click on result");
  let tabPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeMouseAtCenter(element, {});
  await tabPromise;
  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    TEST_URL + "?q=something",
    "Tab should have loaded from clicking on result"
  );

  // Middle-click on the result
  info("Middle-click on result");
  result = await promise_first_result("get somethingmore");

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result"
  );

  assertURL(result, TEST_URL + "?q=somethingmore", "get", "get somethingmore");

  tabPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(element, { button: 1 });
  let tabOpenEvent = await tabPromise;
  let newTab = tabOpenEvent.target;
  await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  Assert.equal(
    newTab.linkedBrowser.currentURI.spec,
    TEST_URL + "?q=somethingmore",
    "Tab should have loaded from middle-clicking on result"
  );
});

add_task(async function test_keyword_using_post() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  let result = await promise_first_result("post something");

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a keyword result"
  );
  Assert.equal(
    result.displayed.title,
    "example.com: something",
    "Node should contain the name of the bookmark and query"
  );
  Assert.ok(!result.displayed.action, "Should have an empty action");

  assertURL(result, TEST_URL, "post", "post something", "q=something");

  // Click on the result
  info("Normal click on result");
  let tabPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(element, {});
  info("waiting for tab");
  await tabPromise;
  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    TEST_URL,
    "Tab should have loaded from clicking on result"
  );

  let postData = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      return content.document.body.textContent;
    }
  );
  Assert.equal(postData, "q=something", "post data was submitted correctly");
});

add_task(async function test_keyword_with_question_mark() {
  // TODO Bug 1517140: keywords containing restriction chars should not be
  // allowed, or properly supported.
  let result = await promise_first_result("question?");
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Result should be a search"
  );
  Assert.equal(result.searchParams.query, "question?", "Check search query");

  result = await promise_first_result("question? something");
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Result should be a keyword"
  );
  Assert.equal(result.keyword, "question?", "Check search query");

  result = await promise_first_result("?question");
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Result should be a search"
  );
  Assert.equal(result.searchParams.query, "question", "Check search query");

  result = await promise_first_result("?question something");
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Result should be a search"
  );
  Assert.equal(
    result.searchParams.query,
    "question something",
    "Check search query"
  );
});
