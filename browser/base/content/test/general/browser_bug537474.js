add_task(async function() {
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  window.browserDOMWindow.openURI(makeURI("about:mozilla"), null,
                                  Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW, null,
                                  Services.scriptSecurityManager.getSystemPrincipal());
  await browserLoadedPromise;
  is(gBrowser.currentURI.spec, "about:mozilla", "page loads in the current content window");
});
