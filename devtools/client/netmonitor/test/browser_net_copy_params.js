/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether copying a request item's parameters works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(PARAMS_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  gStore.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  yield testCopyUrlParamsHidden(0, false);
  yield testCopyUrlParams(0, "a");
  yield testCopyPostDataHidden(0, false);
  yield testCopyPostData(0, "{ \"foo\": \"bar\" }");

  yield testCopyUrlParamsHidden(1, false);
  yield testCopyUrlParams(1, "a=b");
  yield testCopyPostDataHidden(1, false);
  yield testCopyPostData(1, "{ \"foo\": \"bar\" }");

  yield testCopyUrlParamsHidden(2, false);
  yield testCopyUrlParams(2, "a=b");
  yield testCopyPostDataHidden(2, false);
  yield testCopyPostData(2, "foo=bar");

  yield testCopyUrlParamsHidden(3, false);
  yield testCopyUrlParams(3, "a");
  yield testCopyPostDataHidden(3, false);
  yield testCopyPostData(3, "{ \"foo\": \"bar\" }");

  yield testCopyUrlParamsHidden(4, false);
  yield testCopyUrlParams(4, "a=b");
  yield testCopyPostDataHidden(4, false);
  yield testCopyPostData(4, "{ \"foo\": \"bar\" }");

  yield testCopyUrlParamsHidden(5, false);
  yield testCopyUrlParams(5, "a=b");
  yield testCopyPostDataHidden(5, false);
  yield testCopyPostData(5, "?foo=bar");

  yield testCopyUrlParamsHidden(6, true);
  yield testCopyPostDataHidden(6, true);

  return teardown(monitor);

  function testCopyUrlParamsHidden(index, hidden) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]);
    let copyUrlParamsNode = monitor.panelWin.parent.document
      .querySelector("#request-list-context-copy-url-params");
    is(!!copyUrlParamsNode, !hidden,
      "The \"Copy URL Parameters\" context menu item should" + (hidden ? " " : " not ") +
        "be hidden.");
  }

  function* testCopyUrlParams(index, queryString) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]);
    yield waitForClipboardPromise(function setup() {
      monitor.panelWin.parent.document
        .querySelector("#request-list-context-copy-url-params").click();
    }, queryString);
    ok(true, "The url query string copied from the selected item is correct.");
  }

  function testCopyPostDataHidden(index, hidden) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]);
    let copyPostDataNode = monitor.panelWin.parent.document
      .querySelector("#request-list-context-copy-post-data");
    is(!!copyPostDataNode, !hidden,
      "The \"Copy POST Data\" context menu item should" + (hidden ? " " : " not ") +
        "be hidden.");
  }

  function* testCopyPostData(index, postData) {
    // Wait for formDataSections and requestPostData state are ready in redux store
    // since copyPostData API needs to read these state.
    yield waitUntil(() => {
      let { requests } = gStore.getState().requests;
      let actIDs = Object.keys(requests.toJS());
      let { formDataSections, requestPostData } = requests.get(actIDs[index]).toJS();
      return formDataSections && requestPostData;
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[index]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[index]);
    yield waitForClipboardPromise(function setup() {
      monitor.panelWin.parent.document
        .querySelector("#request-list-context-copy-post-data").click();
    }, postData);
    ok(true, "The post data string copied from the selected item is correct.");
  }
});
