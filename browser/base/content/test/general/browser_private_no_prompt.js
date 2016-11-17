function test() {
  waitForExplicitFinish();
  var privateWin = OpenBrowserWindow({private: true});

  whenDelayedStartupFinished(privateWin, function() {
    privateWin.BrowserOpenTab();
    privateWin.BrowserTryToCloseWindow();
    ok(true, "didn't prompt");

    executeSoon(finish);
  });
}
