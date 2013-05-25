/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if focus modifiers work for the SideMenuWidget.
 */

function test() {
  initNetMonitor(CUSTOM_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 2).then(() => {
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

      aDebuggee.performRequests(18);
      return waitForNetworkEvents(aMonitor, 18);
    })
    .then(() => {
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

      teardown(aMonitor).then(finish);
    });

    let count = 0;

    function check(aSelectedIndex, aPaneVisibility) {
      info("Performing check " + (count++) + ".");

      is(RequestsMenu.selectedIndex, aSelectedIndex,
        "The selected item in the requests menu was incorrect.");
      is(NetMonitorView.detailsPaneHidden, !aPaneVisibility,
        "The network requests details pane visibility state was incorrect.");
    }

    aDebuggee.performRequests(2);
  });
}
