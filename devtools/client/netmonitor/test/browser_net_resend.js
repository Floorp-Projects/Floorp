/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a request works.
 */

const ADD_QUERY = "t1=t2";
const ADD_HEADER = "Test-header: true";
const ADD_UA_HEADER = "User-Agent: Custom-Agent";
const ADD_POSTDATA = "&t3=t4";

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { panelWin } = monitor;
  let { document, EVENTS, NetMonitorView } = panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 0, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let origItem = RequestsMenu.getItemAtIndex(0);

  let onTabUpdated = panelWin.once(EVENTS.TAB_UPDATED);
  RequestsMenu.selectedItem = origItem;
  yield onTabUpdated;

  // add a new custom request cloned from selected request
  let onPopulated = panelWin.once(EVENTS.CUSTOMREQUESTVIEW_POPULATED);
  RequestsMenu.cloneSelectedRequest();
  yield onPopulated;

  testCustomForm(origItem);

  let customItem = RequestsMenu.selectedItem;
  testCustomItem(customItem, origItem);

  // edit the custom request
  yield editCustomForm();
  // FIXME: reread the customItem, it's been replaced by a new object (immutable!)
  customItem = RequestsMenu.selectedItem;
  testCustomItemChanged(customItem, origItem);

  // send the new request
  wait = waitForNetworkEvents(monitor, 0, 1);
  RequestsMenu.sendCustomRequest();
  yield wait;

  let sentItem = RequestsMenu.selectedItem;
  testSentRequest(sentItem, origItem);

  return teardown(monitor);

  function testCustomItem(item, orig) {
    is(item.method, orig.method, "item is showing the same method as original request");
    is(item.url, orig.url, "item is showing the same URL as original request");
  }

  function testCustomItemChanged(item, orig) {
    let url = item.url;
    let expectedUrl = orig.url + "&" + ADD_QUERY;

    is(url, expectedUrl, "menu item is updated to reflect url entered in form");
  }

  /*
   * Test that the New Request form was populated correctly
   */
  function testCustomForm(data) {
    is(document.getElementById("custom-method-value").value, data.method,
       "new request form showing correct method");

    is(document.getElementById("custom-url-value").value, data.url,
       "new request form showing correct url");

    let query = document.getElementById("custom-query-value");
    is(query.value, "foo=bar\nbaz=42\ntype=urlencoded",
       "new request form showing correct query string");

    let headers = document.getElementById("custom-headers-value").value.split("\n");
    for (let {name, value} of data.requestHeaders.headers) {
      ok(headers.indexOf(name + ": " + value) >= 0, "form contains header from request");
    }

    let postData = document.getElementById("custom-postdata-value");
    is(postData.value, data.requestPostData.postData.text,
       "new request form showing correct post data");
  }

  /*
   * Add some params and headers to the request form
   */
  function* editCustomForm() {
    panelWin.focus();

    let query = document.getElementById("custom-query-value");
    let queryFocus = once(query, "focus", false);
    // Bug 1195825: Due to some unexplained dark-matter with promise,
    // focus only works if delayed by one tick.
    executeSoon(() => query.focus());
    yield queryFocus;

    // add params to url query string field
    type(["VK_RETURN"]);
    type(ADD_QUERY);

    let headers = document.getElementById("custom-headers-value");
    let headersFocus = once(headers, "focus", false);
    headers.focus();
    yield headersFocus;

    // add a header
    type(["VK_RETURN"]);
    type(ADD_HEADER);

    // add a User-Agent header, to check if default headers can be modified
    // (there will be two of them, first gets overwritten by the second)
    type(["VK_RETURN"]);
    type(ADD_UA_HEADER);

    let postData = document.getElementById("custom-postdata-value");
    let postFocus = once(postData, "focus", false);
    postData.focus();
    yield postFocus;

    // add to POST data
    type(ADD_POSTDATA);
  }

  /*
   * Make sure newly created event matches expected request
   */
  function testSentRequest(data, origData) {
    is(data.method, origData.method, "correct method in sent request");
    is(data.url, origData.url + "&" + ADD_QUERY, "correct url in sent request");

    let { headers } = data.requestHeaders;
    let hasHeader = headers.some(h => `${h.name}: ${h.value}` == ADD_HEADER);
    ok(hasHeader, "new header added to sent request");

    let hasUAHeader = headers.some(h => `${h.name}: ${h.value}` == ADD_UA_HEADER);
    ok(hasUAHeader, "User-Agent header added to sent request");

    is(data.requestPostData.postData.text,
       origData.requestPostData.postData.text + ADD_POSTDATA,
       "post data added to sent request");
  }

  function type(string) {
    for (let ch of string) {
      EventUtils.synthesizeKey(ch, {}, panelWin);
    }
  }
});
