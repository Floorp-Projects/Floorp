var listener = {
  handleEvent : function(e) {
    if (e.target == theBrowser.contentDocument)
      doTest();
  }
}

var theTab;
var theBrowser;

function test() {
  waitForExplicitFinish();

  theTab = gBrowser.addTab();
  theBrowser = gBrowser.getBrowserForTab(theTab);
  theBrowser.addEventListener("load", listener, true);
  
  var rootDir = getRootDirectory(gTestPath);
  theBrowser.contentWindow.location = rootDir + "bug453896_iframe.html";
}

function doTest() {
  theBrowser.removeEventListener("load", listener, true);
  var fake_window = { ok: ok, SimpleTest: { finish: finish } };
  theBrowser.contentWindow.wrappedJSObject.run(fake_window);
  gBrowser.removeTab(theTab);
}
