function test() {
  waitForExplicitFinish();

  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");

  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    win.content.addEventListener("focus", function () {
      win.content.removeEventListener("focus", arguments.callee, false);

      function onTabClose() {
        ok(false, "shouldn't have gotten the TabClose event for the last tab");
      }
      var tab = win.gBrowser.selectedTab;
      tab.addEventListener("TabClose", onTabClose, false);

      EventUtils.synthesizeKey("w", { accelKey: true }, win);

      ok(win.closed, "accel+w closed the window immediately");

      tab.removeEventListener("TabClose", onTabClose, false);

      finish();
    }, false);
  }, false);
}
