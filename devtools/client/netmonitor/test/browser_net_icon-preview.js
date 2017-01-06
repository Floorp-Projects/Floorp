/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if image responses show a thumbnail in the requests menu.
 */

add_task(function* () {
  let Actions = require("devtools/client/netmonitor/actions/index");

  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { $, $all, EVENTS, ACTIVITY_TYPE, NetMonitorView, NetMonitorController,
        gStore } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForEvents();
  yield performRequests();
  yield wait;

  info("Checking the image thumbnail when all items are shown.");
  checkImageThumbnail();

  gStore.dispatch(Actions.sortBy("size"));
  info("Checking the image thumbnail when all items are sorted.");
  checkImageThumbnail();

  gStore.dispatch(Actions.toggleRequestFilterType("images"));
  info("Checking the image thumbnail when only images are shown.");
  checkImageThumbnail();

  info("Reloading the debuggee and performing all requests again...");
  wait = waitForEvents();
  yield reloadAndPerformRequests();
  yield wait;

  info("Checking the image thumbnail after a reload.");
  checkImageThumbnail();

  yield teardown(monitor);

  function waitForEvents() {
    return promise.all([
      waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS),
      monitor.panelWin.once(EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
    ]);
  }

  function performRequests() {
    return ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.wrappedJSObject.performRequests();
    });
  }

  function* reloadAndPerformRequests() {
    yield NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
    yield performRequests();
  }

  function checkImageThumbnail() {
    is($all(".requests-menu-icon[data-type=thumbnail]").length, 1,
      "There should be only one image request with a thumbnail displayed.");
    is($(".requests-menu-icon[data-type=thumbnail]").src, TEST_IMAGE_DATA_URI,
      "The image requests-menu-icon thumbnail is displayed correctly.");
    is($(".requests-menu-icon[data-type=thumbnail]").hidden, false,
      "The image requests-menu-icon thumbnail should not be hidden.");
  }
});
