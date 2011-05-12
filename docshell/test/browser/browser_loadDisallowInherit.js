/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  let browser = gBrowser.getBrowserForTab(tab);

  function loadURL(url, flags, func) {
    browser.addEventListener("load", function loadListener(e) {
      if (browser.currentURI.spec != url)
        return;
      browser.removeEventListener(e.type, loadListener, true);
      func();
    }, true);
    browser.loadURIWithFlags(url, flags, null, null, null);
  }

  // Load a normal http URL
  loadURL("http://example.com/", 0, function () {
    let pagePrincipal = browser.contentPrincipal;

    // Now load a data URI normally
    loadURL("data:text/html,<body>inherit", 0, function () {
      let dataPrincipal = browser.contentPrincipal;
      ok(dataPrincipal.equals(pagePrincipal), "data URI should inherit principal");

      // Load a normal http URL
      loadURL("http://example.com/", 0, function () {
        let innerPagePrincipal = browser.contentPrincipal;

        // Now load a data URI and disallow inheriting the principal
        let webNav = Components.interfaces.nsIWebNavigation;
        loadURL("data:text/html,<body>noinherit", webNav.LOAD_FLAGS_DISALLOW_INHERIT_OWNER, function () {
          let innerDataPrincipal = browser.contentPrincipal;
          ok(!innerDataPrincipal.equals(innerPagePrincipal), "data URI should not inherit principal");

          finish();
        });
      });
    });
  });
}
