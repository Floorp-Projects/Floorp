/* Any copyright is dedicated to the Public Domain.
*  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a request works.
 */

const ADD_QUERY = "t1=t2";
const ADD_HEADER = "Test-header: true";
const ADD_UA_HEADER = "User-Agent: Custom-Agent";
const ADD_POSTDATA = "&t3=t4";

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, store, windowRequire, connector } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getSelectedRequest,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  let origItemId = getSortedRequests(store.getState()).get(0).id;

  store.dispatch(Actions.selectRequest(origItemId));
  await waitForRequestData(store, ["requestHeaders", "requestPostData"], origItemId);

  let origItem = getSortedRequests(store.getState()).get(0);

  // add a new custom request cloned from selected request

  store.dispatch(Actions.cloneSelectedRequest());
  await testCustomForm(origItem);

  let customItem = getSelectedRequest(store.getState());
  testCustomItem(customItem, origItem);

  // edit the custom request
  await editCustomForm();

  // FIXME: reread the customItem, it's been replaced by a new object (immutable!)
  customItem = getSelectedRequest(store.getState());
  testCustomItemChanged(customItem, origItem);

  // send the new request
  wait = waitForNetworkEvents(monitor, 1);
  store.dispatch(Actions.sendCustomRequest(connector));
  await wait;

  let sentItem;
  // Testing sent request will require updated requestHeaders and requestPostData,
  // we must wait for both properties get updated before starting test.
  await waitUntil(() => {
    sentItem = getSelectedRequest(store.getState());
    origItem = getSortedRequests(store.getState()).get(0);
    return sentItem.requestHeaders && sentItem.requestPostData &&
      origItem.requestHeaders && origItem.requestPostData;
  });

  await testSentRequest(sentItem, origItem);

  // Ensure the UI shows the new request, selected, and that the detail panel was closed.
  is(getSortedRequests(store.getState()).length, 3, "There are 3 requests shown");
  is(document.querySelector(".request-list-item.selected").getAttribute("data-id"),
    sentItem.id, "The sent request is selected");
  is(document.querySelector(".network-details-panel"), null,
    "The detail panel is hidden");

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
  async function testCustomForm(data) {
    await waitUntil(() => document.querySelector(".custom-request-panel"));
    is(document.getElementById("custom-method-value").value, data.method,
       "new request form showing correct method");

    is(document.getElementById("custom-url-value").value, data.url,
       "new request form showing correct url");

    let query = document.getElementById("custom-query-value");
    is(query.value, "foo=bar\nbaz=42\ntype=urlencoded",
       "new request form showing correct query string");

    let headers = document.getElementById("custom-headers-value").value.split("\n");
    for (let {name, value} of data.requestHeaders.headers) {
      ok(headers.includes(name + ": " + value), "form contains header from request");
    }

    let postData = document.getElementById("custom-postdata-value");
    is(postData.value, data.requestPostData.postData.text,
       "new request form showing correct post data");
  }

  /*
   * Add some params and headers to the request form
   */
  async function editCustomForm() {
    monitor.panelWin.focus();

    let query = document.getElementById("custom-query-value");
    let queryFocus = once(query, "focus", false);
    // Bug 1195825: Due to some unexplained dark-matter with promise,
    // focus only works if delayed by one tick.
    query.setSelectionRange(query.value.length, query.value.length);
    executeSoon(() => query.focus());
    await queryFocus;

    // add params to url query string field
    type(["VK_RETURN"]);
    type(ADD_QUERY);

    let headers = document.getElementById("custom-headers-value");
    let headersFocus = once(headers, "focus", false);
    headers.setSelectionRange(headers.value.length, headers.value.length);
    headers.focus();
    await headersFocus;

    // add a header
    type(["VK_RETURN"]);
    type(ADD_HEADER);

    // add a User-Agent header, to check if default headers can be modified
    // (there will be two of them, first gets overwritten by the second)
    type(["VK_RETURN"]);
    type(ADD_UA_HEADER);

    let postData = document.getElementById("custom-postdata-value");
    let postFocus = once(postData, "focus", false);
    postData.setSelectionRange(postData.value.length, postData.value.length);
    postData.focus();
    await postFocus;

    // add to POST data once textarea has updated
    await waitUntil(() => postData.textContent !== "");
    type(ADD_POSTDATA);
  }

  /*
   * Make sure newly created event matches expected request
   */
  async function testSentRequest(data, origData) {
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
      EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
    }
  }
});
