// tests third party cookie blocking using a favicon load directly from chrome.
// in this case, the docshell of the channel is chrome, not content; thus
// the cookie should be considered third party.

function test() {
  waitForExplicitFinish();

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.setIntPref("network.cookie.cookieBehavior", 1);

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.addObserver(function (theSubject, theTopic, theData) {
    var uri = theSubject.QueryInterface(Components.interfaces.nsIURI);
    var domain = uri.host;

    if (domain == "example.org") {
      ok(true, "foreign favicon cookie was blocked");

      var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch);
      prefs.setIntPref("network.cookie.cookieBehavior", 0);

      os.removeObserver(arguments.callee, "cookie-rejected");

      finish();
    }
  }, "cookie-rejected", false);

  // kick off a favicon load
  PageProxySetIcon("http://example.org/tests/extensions/cookie/test/image1.png");
}
