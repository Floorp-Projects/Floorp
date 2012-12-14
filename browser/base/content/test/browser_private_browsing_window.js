// Make sure that we can open private browsing windows

function test() {
  waitForExplicitFinish();
  var nonPrivateWin = OpenBrowserWindow();
  ok(!PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin), "OpenBrowserWindow() should open a normal window");
  nonPrivateWin.close();

  var privateWin = OpenBrowserWindow({private: true});
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWin), "OpenBrowserWindow({private: true}) should open a private window");

  nonPrivateWin = OpenBrowserWindow({private: false});
  ok(!PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin), "OpenBrowserWindow({private: false}) should open a normal window");
  nonPrivateWin.close();

  whenDelayedStartupFinished(privateWin, function() {
    nonPrivateWin = privateWin.OpenBrowserWindow({private: false});
    ok(!PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin), "privateWin.OpenBrowserWindow({private: false}) should open a normal window");
    nonPrivateWin.close();
    privateWin.close();
    finish();
  });
}

