function test() {
  var tabCount = gBrowser.tabs.length;
  gBrowser.selectedBrowser.focus();
  window.browserDOMWindow.openURI(makeURI("about:blank"),
                                  null,
                                  Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                                  Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
                                  Services.scriptSecurityManager.getSystemPrincipal());
  is(gBrowser.tabs.length, tabCount + 1,
     "'--new-tab about:blank' opens a new tab");
  is(gBrowser.selectedTab, gBrowser.tabs[tabCount],
     "'--new-tab about:blank' selects the new tab");
  is(document.activeElement, gURLBar.inputField,
     "'--new-tab about:blank' focuses the location bar");
  gBrowser.removeCurrentTab();
}
