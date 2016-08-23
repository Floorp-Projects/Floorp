/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if focus modifiers work for the SideMenuWidget.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  // It seems that this test may be slow on Ubuntu builds running on ec2.
  requestLongerTimeout(2);

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let count = 0;
  function check(selectedIndex, paneVisibility) {
    info("Performing check " + (count++) + ".");

    is(RequestsMenu.selectedIndex, selectedIndex,
      "The selected item in the requests menu was incorrect.");
    is(NetMonitorView.detailsPaneHidden, !paneVisibility,
      "The network requests details pane visibility state was incorrect.");
  }

  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(2);
  });
  yield wait;

  check(-1, false);

  RequestsMenu.focusLastVisibleItem();
  check(1, true);
  RequestsMenu.focusFirstVisibleItem();
  check(0, true);

  RequestsMenu.focusNextItem();
  check(1, true);
  RequestsMenu.focusPrevItem();
  check(0, true);

  RequestsMenu.focusItemAtDelta(+1);
  check(1, true);
  RequestsMenu.focusItemAtDelta(-1);
  check(0, true);

  RequestsMenu.focusItemAtDelta(+10);
  check(1, true);
  RequestsMenu.focusItemAtDelta(-10);
  check(0, true);

  wait = waitForNetworkEvents(monitor, 18);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(18);
  });
  yield wait;

  RequestsMenu.focusLastVisibleItem();
  check(19, true);
  RequestsMenu.focusFirstVisibleItem();
  check(0, true);

  RequestsMenu.focusNextItem();
  check(1, true);
  RequestsMenu.focusPrevItem();
  check(0, true);

  RequestsMenu.focusItemAtDelta(+10);
  check(10, true);
  RequestsMenu.focusItemAtDelta(-10);
  check(0, true);

  RequestsMenu.focusItemAtDelta(+100);
  check(19, true);
  RequestsMenu.focusItemAtDelta(-100);
  check(0, true);

  yield teardown(monitor);
});
