// tests third party cookie blocking using a favicon load directly from chrome.
// in this case, the docshell of the channel is chrome, not content; thus
// the cookie should be considered third party.

function test() {
  waitForExplicitFinish();

  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);

  Services.obs.addObserver(function (theSubject, theTopic, theData) {
    var uri = theSubject.QueryInterface(Components.interfaces.nsIURI);
    var domain = uri.host;

    if (domain == "example.org") {
      ok(true, "foreign favicon cookie was blocked");

      Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

      Services.obs.removeObserver(arguments.callee, "cookie-rejected");

      finish();
    }
  }, "cookie-rejected", false);

  // kick off a favicon load
  gBrowser.setIcon(gBrowser.selectedTab, "http://example.org/tests/extensions/cookie/test/damonbowling.jpg");
}
