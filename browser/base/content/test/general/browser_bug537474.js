add_task(function *() {
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  browserDOMWindow.openURI(makeURI("about:"), null,
                           Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW, null)
  yield browserLoadedPromise;
  is(gBrowser.currentURI.spec, "about:", "page loads in the current content window");
});

