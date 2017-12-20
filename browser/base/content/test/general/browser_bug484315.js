function test() {
  var contentWin = window.open("about:blank", "", "width=100,height=100");
  var enumerator = Services.wm.getEnumerator("navigator:browser");

  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
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

  throw "couldn't find the content window";
}
