function test() {
  waitForExplicitFinish();

  whenNewWindowLoaded(null, function(win) {
    waitForFocus(function() {
      function onTabClose() {
        ok(false, "shouldn't have gotten the TabClose event for the last tab");
      }
      var tab = win.gBrowser.selectedTab;
      tab.addEventListener("TabClose", onTabClose, false);

      EventUtils.synthesizeKey("w", { accelKey: true }, win);

      ok(win.closed, "accel+w closed the window immediately");

      tab.removeEventListener("TabClose", onTabClose, false);

      finish();
    }, win);
  });
}
