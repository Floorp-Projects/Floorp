function test() {
  const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com");
  const URI = TEST_PATH + "file_bug562649.html";
  window.browserDOMWindow.openURI(makeURI(URI),
                                  null,
                                  Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                                  Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL,
                                  Services.scriptSecurityManager.getSystemPrincipal());

  is(gBrowser.userTypedValue, URI, "userTypedValue matches test URI");
  is(gURLBar.value, URI, "location bar value matches test URI");

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.removeCurrentTab({ skipPermitUnload: true });
  is(gBrowser.userTypedValue, URI, "userTypedValue matches test URI after switching tabs");
  is(gURLBar.value, URI, "location bar value matches test URI after switching tabs");

  waitForExplicitFinish();
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    is(gBrowser.userTypedValue, null, "userTypedValue is null as the page has loaded");
    is(gURLBar.value, URI, "location bar value matches test URI as the page has loaded");

    gBrowser.removeCurrentTab({ skipPermitUnload: true });
    finish();
  });
}
