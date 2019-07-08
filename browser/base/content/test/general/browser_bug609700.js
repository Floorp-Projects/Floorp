function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(function notification(
    aSubject,
    aTopic,
    aData
  ) {
    if (aTopic == "domwindowopened") {
      Services.ww.unregisterNotification(notification);

      ok(true, "duplicateTabIn opened a new window");

      whenDelayedStartupFinished(
        aSubject,
        function() {
          executeSoon(function() {
            aSubject.close();
            finish();
          });
        },
        false
      );
    }
  });

  duplicateTabIn(gBrowser.selectedTab, "window");
}
