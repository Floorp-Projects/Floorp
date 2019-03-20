function test() {
  var contentWin = window.open("about:blank", "", "width=100,height=100");
  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    if (win.content == contentWin) {
      Services.prefs.setBoolPref("browser.tabs.closeWindowWithLastTab", false);
      win.gBrowser.removeCurrentTab();
      ok(win.closed, "popup is closed");

      // clean up
      if (!win.closed)
        win.close();
      if (Services.prefs.prefHasUserValue("browser.tabs.closeWindowWithLastTab"))
        Services.prefs.clearUserPref("browser.tabs.closeWindowWithLastTab");

      return;
    }
  }

  throw new Error("couldn't find the content window");
}
