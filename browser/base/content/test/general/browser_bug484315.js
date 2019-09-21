add_task(async function test() {
  window.open("about:blank", "", "width=100,height=100,noopener");

  let win = Services.wm.getMostRecentWindow("navigator:browser");
  Services.prefs.setBoolPref("browser.tabs.closeWindowWithLastTab", false);
  win.gBrowser.removeCurrentTab();
  ok(win.closed, "popup is closed");

  // clean up
  if (!win.closed) {
    win.close();
  }
  Services.prefs.clearUserPref("browser.tabs.closeWindowWithLastTab");
});
