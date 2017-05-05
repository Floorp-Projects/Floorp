/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if focus modifiers work for the SideMenuWidget.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  // It seems that this test may be slow on Ubuntu builds running on ec2.
  requestLongerTimeout(2);

  let { document, store, windowRequire } = monitor.panelWin;
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

  check(-1, false);

  store.dispatch(Actions.selectDelta(+Infinity));
  check(1, true);
  store.dispatch(Actions.selectDelta(-Infinity));
  check(0, true);

  store.dispatch(Actions.selectDelta(+1));
  check(1, true);
  store.dispatch(Actions.selectDelta(-1));
  check(0, true);

  store.dispatch(Actions.selectDelta(+10));
  check(1, true);
  store.dispatch(Actions.selectDelta(-10));
  check(0, true);

  wait = waitForNetworkEvents(monitor, 18);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(18);
  });
  yield wait;

  store.dispatch(Actions.selectDelta(+Infinity));
  check(19, true);
  store.dispatch(Actions.selectDelta(-Infinity));
  check(0, true);

  store.dispatch(Actions.selectDelta(+1));
  check(1, true);
  store.dispatch(Actions.selectDelta(-1));
  check(0, true);

  store.dispatch(Actions.selectDelta(+10));
  check(10, true);
  store.dispatch(Actions.selectDelta(-10));
  check(0, true);

  store.dispatch(Actions.selectDelta(+100));
  check(19, true);
  store.dispatch(Actions.selectDelta(-100));
  check(0, true);

  return teardown(monitor);
});
