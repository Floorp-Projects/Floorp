function test() {
  waitForExplicitFinish();
  var privateWin = OpenBrowserWindow({ private: true });

  whenDelayedStartupFinished(privateWin, function () {
    privateWin.BrowserCommands.openTab();
    privateWin.BrowserCommands.tryToCloseWindow();
    ok(true, "didn't prompt");

    executeSoon(finish);
  });
}
