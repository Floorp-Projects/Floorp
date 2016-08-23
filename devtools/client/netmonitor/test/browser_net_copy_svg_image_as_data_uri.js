/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

const SVG_URL = EXAMPLE_URL + "dropmarker.svg";

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CURL_URL);
  info("Starting test... ");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, SVG_URL, function* (url) {
    content.wrappedJSObject.performRequest(url);
  });
  yield wait;

  let requestItem = RequestsMenu.getItemAtIndex(0);
  RequestsMenu.selectedItem = requestItem;

  yield waitForClipboardPromise(function setup() {
    RequestsMenu.copyImageAsDataUri();
  }, function check(text) {
    return text.startsWith("data:") && !/undefined/.test(text);
  });

  yield teardown(monitor);
});
