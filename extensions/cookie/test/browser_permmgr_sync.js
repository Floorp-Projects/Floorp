function addPerm(aURI, aName) {
  Services.perms.add(Services.io.newURI(aURI), aName, Services.perms.ALLOW_ACTION);
}

function hasPerm(aURI, aName) {
  return Services.perms.testPermission(Services.io.newURI(aURI), aName)
    == Services.perms.ALLOW_ACTION;
}

add_task(function* () {
  // Make sure that we get a new process for the tab which we create. This is
  // important, becuase we wanto to assert information about the initial state
  // of the local permissions cache.
  //
  // We use the same approach here as was used in the e10s-multi localStorage
  // tests (dom/tests/browser/browser_localStorage_e10s.js (bug )). This ensures
  // that our tab has its own process.
  //
  // Bug 1345990 tracks implementing a better tool for ensuring this.
  let keepAliveCount = 0;
  try {
    keepAliveCount = SpecialPowers.getIntPref("dom.ipc.keepProcessesAlive.web");
  } catch (ex) {
    // Then zero is correct.
  }
  let safeProcessCount = keepAliveCount + 2;
  info("dom.ipc.keepProcessesAlive.web is " + keepAliveCount + ", boosting " +
       "process count temporarily to " + safeProcessCount);
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", safeProcessCount],
      ["dom.ipc.processCount.web", safeProcessCount]
    ]
  });

  addPerm("http://example.com", "perm1");
  addPerm("http://foo.bar.example.com", "perm2");
  addPerm("about:home", "perm3");
  addPerm("https://example.com", "perm4");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function* (aBrowser) {
    yield ContentTask.spawn(aBrowser, null, function* () {
      // Before the load http URIs shouldn't have been sent down yet
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION);

      // Perform a load of example.com
      yield new Promise(resolve => {
        let iframe = content.document.createElement('iframe');
        iframe.setAttribute('src', 'http://example.com');
        iframe.onload = resolve;
        content.document.body.appendChild(iframe);
      });

      // After the load finishes, we should know about example.com, but not foo.bar.example.com
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION);
    });

    addPerm("http://example.com", "newperm1");
    addPerm("http://foo.bar.example.com", "newperm2");
    addPerm("about:home", "newperm3");
    addPerm("https://example.com", "newperm4");

    yield ContentTask.spawn(aBrowser, null, function* () {
      // The new permissions should be avaliable, but only for
      // http://example.com, and about:home
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "newperm1"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "newperm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "newperm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "newperm4"),
         Services.perms.UNKNOWN_ACTION);

      // Loading a subdomain now, on https
      yield new Promise(resolve => {
        let iframe = content.document.createElement('iframe');
        iframe.setAttribute('src', 'https://sub1.test1.example.com');
        iframe.onload = resolve;
        content.document.body.appendChild(iframe);
      });

      // Now that the https subdomain has loaded, we want to make sure that the
      // permissions are also avaliable for its parent domain, https://example.com!
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "newperm1"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "newperm2"),
         Services.perms.UNKNOWN_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "newperm3"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.ALLOW_ACTION);
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "newperm4"),
         Services.perms.ALLOW_ACTION);
    });
  });
});
