function test() {
  var currentWin = content;
  var newWin =
    browserDOMWindow.openURI(makeURI("about:"), null,
                             Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW, null)
  is(newWin, currentWin, "page loads in the current content window");
  gBrowser.stop();
}
