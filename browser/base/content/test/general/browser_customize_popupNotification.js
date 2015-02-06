/*
Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/
*/

function test() {
  waitForExplicitFinish();
  let newWin = OpenBrowserWindow();
  registerCleanupFunction(() => {
    newWin.close()
    newWin = null;
  });
  whenDelayedStartupFinished(newWin, function () {
    // Remove the URL bar
    newWin.gURLBar.parentNode.removeChild(newWin.gURLBar);

    waitForFocus(function () {
      let PN = newWin.PopupNotifications;
      try {
        let panelPromise = promisePopupShown(PN.panel);
        let notification = PN.show(newWin.gBrowser.selectedBrowser, "some-notification", "Some message");
        panelPromise.then(function() {
          ok(notification, "showed the notification");
          ok(PN.isPanelOpen, "panel is open");
          is(PN.panel.anchorNode, newWin.gBrowser.selectedTab, "notification is correctly anchored to the tab");
          PN.panel.hidePopup();
          finish();
        });
      } catch (ex) {
        ok(false, "threw exception: " + ex);
        finish();
      }
    }, newWin);
  });
}
