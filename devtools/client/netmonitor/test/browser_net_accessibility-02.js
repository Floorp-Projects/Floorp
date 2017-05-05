/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if keyboard and mouse navigation works in the network requests menu.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  // It seems that this test may be slow on Ubuntu builds running on ec2.
  requestLongerTimeout(2);

  let { window, document, windowRequire, store } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  let count = 0;
  function check(selectedIndex, panelVisibility) {
    info("Performing check " + (count++) + ".");

    let requestItems = Array.from(document.querySelectorAll(".request-list-item"));
    is(requestItems.findIndex((item) => item.matches(".selected")), selectedIndex,
      "The selected item in the requests menu was incorrect.");
    is(!!document.querySelector(".network-details-panel"), panelVisibility,
      "The network details panel should render correctly.");
  }

  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(2);
  });
  yield wait;

  document.querySelector(".requests-list-contents").focus();

  check(-1, false);

  EventUtils.sendKey("DOWN", window);
  check(0, true);
  EventUtils.sendKey("UP", window);
  check(0, true);

  EventUtils.sendKey("PAGE_DOWN", window);
  check(1, true);
  EventUtils.sendKey("PAGE_UP", window);
  check(0, true);

  EventUtils.sendKey("END", window);
  check(1, true);
  EventUtils.sendKey("HOME", window);
  check(0, true);

  wait = waitForNetworkEvents(monitor, 18);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(18);
  });
  yield wait;

  EventUtils.sendKey("DOWN", window);
  check(1, true);
  EventUtils.sendKey("DOWN", window);
  check(2, true);
  EventUtils.sendKey("UP", window);
  check(1, true);
  EventUtils.sendKey("UP", window);
  check(0, true);

  EventUtils.sendKey("PAGE_DOWN", window);
  check(4, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(8, true);
  EventUtils.sendKey("PAGE_UP", window);
  check(4, true);
  EventUtils.sendKey("PAGE_UP", window);
  check(0, true);

  EventUtils.sendKey("HOME", window);
  check(0, true);
  EventUtils.sendKey("HOME", window);
  check(0, true);
  EventUtils.sendKey("PAGE_UP", window);
  check(0, true);
  EventUtils.sendKey("HOME", window);
  check(0, true);

  EventUtils.sendKey("END", window);
  check(19, true);
  EventUtils.sendKey("END", window);
  check(19, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(19, true);
  EventUtils.sendKey("END", window);
  check(19, true);

  EventUtils.sendKey("PAGE_UP", window);
  check(15, true);
  EventUtils.sendKey("PAGE_UP", window);
  check(11, true);
  EventUtils.sendKey("UP", window);
  check(10, true);
  EventUtils.sendKey("UP", window);
  check(9, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(13, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(17, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(19, true);
  EventUtils.sendKey("PAGE_DOWN", window);
  check(19, true);

  EventUtils.sendKey("HOME", window);
  check(0, true);
  EventUtils.sendKey("DOWN", window);
  check(1, true);
  EventUtils.sendKey("END", window);
  check(19, true);
  EventUtils.sendKey("DOWN", window);
  check(19, true);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector(".request-list-item"));
  check(0, true);

  yield teardown(monitor);
});
