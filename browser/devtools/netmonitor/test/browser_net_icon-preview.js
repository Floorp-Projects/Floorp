/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if image responses show a thumbnail in the requests menu.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, $all, EVENTS, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    promise.all([
      waitForNetworkEvents(aMonitor, 6),
      waitFor(aMonitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
    ]).then(() => {
      is($all(".requests-menu-icon[type=thumbnail]").length, 1,
        "There should be only one image request with a thumbnail displayed.");
      is($(".requests-menu-icon[type=thumbnail]").src, TEST_IMAGE_DATA_URI,
        "The image requests-menu-icon thumbnail is displayed correctly.");
      is($(".requests-menu-icon[type=thumbnail]").hidden, false,
        "The image requests-menu-icon thumbnail should not be hidden.");

      teardown(aMonitor).then(finish);
    });

    aDebuggee.performRequests();
  });
}
