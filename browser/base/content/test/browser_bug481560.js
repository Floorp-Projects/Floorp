function test() {
  waitForExplicitFinish();

  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");

  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    win.content.addEventListener("focus", function () {
      win.content.removeEventListener("focus", arguments.callee, false);

      win.gBrowser.selectedTab.addEventListener("TabClose", function () {
        ok(false, "shouldn't have gotten the TabClose event for the last tab");
      }, false);

      EventUtils.synthesizeKey("w", { accelKey: true }, win);

      ok(win.closed, "accel+w closed the window immediately");

      finish();
    }, false);
  }, false);
}
