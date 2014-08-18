/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if keyboard and mouse navigation works in the network requests menu.
 */

function test() {
  initNetMonitor(CUSTOM_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    // It seems that this test may be slow on Ubuntu builds running on ec2.
    requestLongerTimeout(2);

    let { window, $, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 2).then(() => {
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

      aDebuggee.performRequests(18);
      return waitForNetworkEvents(aMonitor, 18);
    })
    .then(() => {
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

      EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));
      check(-1, false);

      EventUtils.sendMouseEvent({ type: "mousedown" }, $(".side-menu-widget-item"));
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
