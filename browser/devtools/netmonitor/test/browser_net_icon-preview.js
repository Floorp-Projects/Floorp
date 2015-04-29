/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if image responses show a thumbnail in the requests menu.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, $all, EVENTS, ACTIVITY_TYPE, NetMonitorView, NetMonitorController } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    promise.all([
      waitForNetworkEvents(aMonitor, 7),
      waitFor(aMonitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
    ]).then(() => {
      info("Checking the image thumbnail when all items are shown.");
      checkImageThumbnail();

      RequestsMenu.sortBy("size");
      info("Checking the image thumbnail when all items are sorted.");
      checkImageThumbnail();

      RequestsMenu.filterOn("images");
      info("Checking the image thumbnail when only images are shown.");
      checkImageThumbnail();

      info("Reloading the debuggee and performing all requests again...");
      reloadAndPerformRequests();

      return promise.all([
        waitForNetworkEvents(aMonitor, 7), // 6 + 1
        waitFor(aMonitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
      ]);
    }).then(() => {
      info("Checking the image thumbnail after a reload.");
      checkImageThumbnail();

      teardown(aMonitor).then(finish);
    });

    function reloadAndPerformRequests() {
      NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED).then(() => {
        aDebuggee.performRequests();
      });
    }

    function checkImageThumbnail() {
      is($all(".requests-menu-icon[type=thumbnail]").length, 1,
        "There should be only one image request with a thumbnail displayed.");
      is($(".requests-menu-icon[type=thumbnail]").src, TEST_IMAGE_DATA_URI,
        "The image requests-menu-icon thumbnail is displayed correctly.");
      is($(".requests-menu-icon[type=thumbnail]").hidden, false,
        "The image requests-menu-icon thumbnail should not be hidden.");
    }

    aDebuggee.performRequests();
  });
}
