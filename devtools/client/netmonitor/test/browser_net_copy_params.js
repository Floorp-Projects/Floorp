/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether copying a request item's parameters works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(PARAMS_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1, 6);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(0);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("{ \"foo\": \"bar\" }");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(1);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a=b");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("{ \"foo\": \"bar\" }");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(2);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a=b");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("foo=bar");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(3);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("{ \"foo\": \"bar\" }");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(4);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a=b");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("{ \"foo\": \"bar\" }");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(5);
  yield testCopyUrlParamsHidden(false);
  yield testCopyUrlParams("a=b");
  yield testCopyPostDataHidden(false);
  yield testCopyPostData("?foo=bar");

  RequestsMenu.selectedItem = RequestsMenu.getItemAtIndex(6);
  yield testCopyUrlParamsHidden(true);
  yield testCopyPostDataHidden(true);

  return teardown(monitor);

  function testCopyUrlParamsHidden(hidden) {
    RequestsMenu._onContextShowing();
    is(document.querySelector("#request-menu-context-copy-url-params").hidden, hidden,
      "The \"Copy URL Parameters\" context menu item should" + (hidden ? " " : " not ") +
        "be hidden.");
  }

  function* testCopyUrlParams(queryString) {
    yield waitForClipboardPromise(function setup() {
      RequestsMenu.copyUrlParams();
    }, queryString);
    ok(true, "The url query string copied from the selected item is correct.");
  }

  function testCopyPostDataHidden(hidden) {
    RequestsMenu._onContextShowing();
    is(document.querySelector("#request-menu-context-copy-post-data").hidden, hidden,
      "The \"Copy POST Data\" context menu item should" + (hidden ? " " : " not ") +
        "be hidden.");
  }

  function* testCopyPostData(postData) {
    yield waitForClipboardPromise(function setup() {
      RequestsMenu.copyPostData();
    }, postData);
    ok(true, "The post data string copied from the selected item is correct.");
  }
});
