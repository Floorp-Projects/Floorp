function test() {
  waitForExplicitFinish();
  var privateWin = OpenBrowserWindow({private: true});
  privateWin.addEventListener("load", function onload() {
    privateWin.removeEventListener("load", onload, false);
    ok(true, "Load listener called");

    privateWin.BrowserOpenTab();
    privateWin.BrowserTryToCloseWindow();
    ok(true, "didn't prompt");
    finish();                        
  }, false);
}