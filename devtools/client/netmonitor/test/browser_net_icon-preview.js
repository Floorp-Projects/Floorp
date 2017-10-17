/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if image responses show a thumbnail in the requests menu.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  const SELECTOR = ".requests-list-icon[src]";
  info("Starting test... ");

  let { document, store, windowRequire, connector } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { triggerActivity } = connector;
  let { ACTIVITY_TYPE } = windowRequire("devtools/client/netmonitor/src/constants");

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield performRequests();
  yield wait;
  yield waitUntil(() => !!document.querySelector(SELECTOR));

  info("Checking the image thumbnail when all items are shown.");
  checkImageThumbnail();

  store.dispatch(Actions.sortBy("contentSize"));
  info("Checking the image thumbnail when all items are sorted.");
  checkImageThumbnail();

  store.dispatch(Actions.toggleRequestFilterType("images"));
  info("Checking the image thumbnail when only images are shown.");
  checkImageThumbnail();

  info("Reloading the debuggee and performing all requests again...");
  wait = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield reloadAndPerformRequests();
  yield wait;
  yield waitUntil(() => !!document.querySelector(SELECTOR));

  info("Checking the image thumbnail after a reload.");
  checkImageThumbnail();

  yield teardown(monitor);

  function performRequests() {
    return ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.wrappedJSObject.performRequests();
    });
  }

  function* reloadAndPerformRequests() {
    yield triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
    yield performRequests();
  }

  function checkImageThumbnail() {
    is(document.querySelectorAll(SELECTOR).length, 1,
      "There should be only one image request with a thumbnail displayed.");
    is(document.querySelector(SELECTOR).src, TEST_IMAGE_DATA_URI,
      "The image requests-list-icon thumbnail is displayed correctly.");
    is(document.querySelector(SELECTOR).hidden, false,
      "The image requests-list-icon thumbnail should not be hidden.");
  }
});
