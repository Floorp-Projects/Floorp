add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (newBrowser) {
    yield ContentTask.spawn(newBrowser, null, function* () {
      var prin = content.document.nodePrincipal;
      isnot(prin, null, "Loaded principal must not be null");
      isnot(prin, undefined, "Loaded principal must not be undefined");

      const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                       .getService(Ci.nsIScriptSecurityManager);
      is(secMan.isSystemPrincipal(prin), false,
         "Loaded principal must not be system");
    });
  });
});

