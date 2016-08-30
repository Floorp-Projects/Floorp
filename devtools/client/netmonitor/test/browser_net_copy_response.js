/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's response works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  const EXPECTED_RESULT = '{ "greeting": "Hello JSON!" }';

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 7);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let requestItem = RequestsMenu.getItemAtIndex(3);
  RequestsMenu.selectedItem = requestItem;

  yield waitForClipboardPromise(function setup() {
    RequestsMenu.copyResponse();
  }, EXPECTED_RESULT);

  yield teardown(monitor);
});
