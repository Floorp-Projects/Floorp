// Make sure that we can open private browsing windows

function test() {
  var nonPrivateWin = OpenBrowserWindow();
  ok(!PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin), "OpenBrowserWindow() should open a normal window");
  nonPrivateWin.close();
  var privateWin = OpenBrowserWindow({private: true});
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWin), "OpenBrowserWindow({private: true}) should open a private window");
  privateWin.close();
}

