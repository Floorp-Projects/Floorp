function test() {
  waitForExplicitFinish(); 

  var newTab;
  var newBrowser;
  const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

  function testLoad(event) {
    newBrowser.removeEventListener("load", testLoad, true);
    is (event.target, newBrowser.contentDocument, "Unexpected target");
    var prin = newBrowser.contentDocument.nodePrincipal;
    isnot(prin, null, "Loaded principal must not be null");
    isnot(prin, undefined, "Loaded principal must not be undefined");
    is(secMan.isSystemPrincipal(prin), false,
       "Loaded principal must not be system");
    gBrowser.removeTab(newTab);

    finish();
  }

  newTab = gBrowser.addTab();
  newBrowser = gBrowser.getBrowserForTab(newTab);
  newBrowser.contentWindow.location.href = "about:blank"
  newBrowser.addEventListener("load", testLoad, true);
}

