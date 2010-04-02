function test() {
  waitForExplicitFinish(); 

  var newTab;
  var newBrowser;
  const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
  var iteration = 1;
  const uris = [undefined, "about:blank"];
  var uri;

  function testLoad(event) {
    newBrowser.removeEventListener("load", testLoad, true);
    is (event.target, newBrowser.contentDocument, "Unexpected target");
    var prin = newBrowser.contentDocument.nodePrincipal;
    isnot(prin, null, "Loaded principal must not be null when adding " + uri);
    isnot(prin, undefined, "Loaded principal must not be undefined when loading " + uri);
    is(secMan.isSystemPrincipal(prin), false,
       "Loaded principal must not be system when loading " + uri);
    gBrowser.removeTab(newTab);

    if (iteration == uris.length) {
      finish();
    } else {
      ++iteration;
      doTest();
    }
  }

  function doTest() {
    uri = uris[iteration - 1];
    newTab = gBrowser.addTab(uri);
    newBrowser = gBrowser.getBrowserForTab(newTab);
    newBrowser.addEventListener("load", testLoad, true);
    var prin = newBrowser.contentDocument.nodePrincipal;
    isnot(prin, null, "Forced principal must not be null when loading " + uri);
    isnot(prin, undefined,
          "Forced principal must not be undefined when loading " + uri);
    is(secMan.isSystemPrincipal(prin), false,
       "Forced principal must not be system when loading " + uri);
   }

   doTest();
}

