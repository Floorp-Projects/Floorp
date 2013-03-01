function test() {
  waitForExplicitFinish();
  var privateWin = OpenBrowserWindow({private: true});
  privateWin.addEventListener("load", function onload() {
    privateWin.removeEventListener("load", onload, false);

    privateWin.BrowserOpenTab();
    privateWin.BrowserTryToCloseWindow();
    ok(true, "didn't prompt");
    finish();
  }, false);
}