function test() {
  var tabs = gBrowser.tabContainer.childElementCount;
  content.focus();
  browserDOMWindow.openURI(makeURI("about:blank"),
                           null,
                           Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                           Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
  is(gBrowser.tabContainer.childElementCount, tabs + 1,
     "'-new-tab about:blank' opens a new tab");
  is(gBrowser.selectedTab, gBrowser.tabContainer.childNodes[tabs],
     "'-new-tab about:blank' selects the new tab");
  is(document.activeElement, gURLBar.inputField,
     "'-new-tab about:blank' focuses the location bar");
  gBrowser.removeCurrentTab();
}
