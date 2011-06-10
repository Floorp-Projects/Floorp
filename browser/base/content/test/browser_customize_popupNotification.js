/*
Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/
*/
function test() {
  waitForExplicitFinish();

  var newWin = openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  registerCleanupFunction(function () {
    newWin.close();
  });
  newWin.addEventListener("load", function test_win_onLoad() {
    newWin.removeEventListener("load", test_win_onLoad, false);

    // Remove the URL bar
    newWin.gURLBar.parentNode.removeChild(newWin.gURLBar);

    waitForFocus(function () {
      let PN = newWin.PopupNotifications;
      try {
        let notification = PN.show(newWin.gBrowser.selectedBrowser, "some-notification", "Some message");
        ok(notification, "showed the notification");
        ok(PN.isPanelOpen, "panel is open");
        is(PN.panel.anchorNode, newWin.gBrowser.selectedTab, "notification is correctly anchored to the tab");
      } catch (ex) {
        ok(false, "threw exception: " + ex);
      }
      finish();
    }, newWin);
  }, false);
}
