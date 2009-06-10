function test() {
  waitForExplicitFinish();

  // focus the url field so that it will can ensure the focus is there when
  // the window is refocused after the dialog closes
  gURLBar.focus();

  window.addEventListener("focus", function () {
    window.removeEventListener("focus", arguments.callee, false);
    finish();
  }, false);

  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");

  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    win.content.addEventListener("focus", function () {
      win.content.removeEventListener("focus", arguments.callee, false);

      EventUtils.synthesizeKey("w", { accelKey: true }, win);
      ok(win.closed, "accel+w closed the window immediately");
    }, false);

    win.gBrowser.selectedTab.addEventListener("TabClose", function () {
      ok(false, "shouldn't have gotten the TabClose event for the last tab");
    }, false);

  }, false);
}
