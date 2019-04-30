function addPerm(aURI, aName) {
  Services.perms.add(Services.io.newURI(aURI), aName, Services.perms.ALLOW_ACTION);
}

function hasPerm(aURI, aName) {
  return Services.perms.testPermission(Services.io.newURI(aURI), aName)
    == Services.perms.ALLOW_ACTION;
}

add_task(async function() {
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
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", safeProcessCount],
      ["dom.ipc.processCount.web", safeProcessCount]
    ]
  });

  addPerm("http://example.com", "perm1");
  addPerm("http://foo.bar.example.com", "perm2");
  addPerm("about:home", "perm3");
  addPerm("https://example.com", "perm4");
  // NOTE: This permission is a preload permission, so it should be avaliable in the content process from startup.
  addPerm("https://somerandomwebsite.com", "document");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, async function(aBrowser) {
    await ContentTask.spawn(aBrowser, null, async function() {
      // Before the load http URIs shouldn't have been sent down yet
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.UNKNOWN_ACTION, "perm1-1");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION, "perm2-1");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION, "perm3-1");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION, "perm4-1");
      is(Services.perms.testPermission(Services.io.newURI("https://somerandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "document-1");

      // Perform a load of example.com
      await new Promise(resolve => {
        let iframe = content.document.createElement('iframe');
        iframe.setAttribute('src', 'http://example.com');
        iframe.onload = resolve;
        content.document.body.appendChild(iframe);
      });

      // After the load finishes, we should know about example.com, but not foo.bar.example.com
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION, "perm1-2");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION, "perm2-2");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION, "perm3-2");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION, "perm4-2");
      is(Services.perms.testPermission(Services.io.newURI("https://somerandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "document-2");
    });

    addPerm("http://example.com", "newperm1");
    addPerm("http://foo.bar.example.com", "newperm2");
    addPerm("about:home", "newperm3");
    addPerm("https://example.com", "newperm4");
    addPerm("https://someotherrandomwebsite.com", "document");

    await ContentTask.spawn(aBrowser, null, async function() {
      // The new permissions should be avaliable, but only for
      // http://example.com, and about:home
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION, "perm1-3");
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "newperm1"),
         Services.perms.ALLOW_ACTION, "newperm1-3");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION, "perm2-3");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "newperm2"),
         Services.perms.UNKNOWN_ACTION, "newperm2-3");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION, "perm3-3");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "newperm3"),
         Services.perms.ALLOW_ACTION, "newperm3-3");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.UNKNOWN_ACTION, "perm4-3");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "newperm4"),
         Services.perms.UNKNOWN_ACTION, "newperm4-3");
      is(Services.perms.testPermission(Services.io.newURI("https://somerandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "document-3");
      is(Services.perms.testPermission(Services.io.newURI("https://someotherrandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "otherdocument-3");

      // Loading a subdomain now, on https
      await new Promise(resolve => {
        let iframe = content.document.createElement('iframe');
        iframe.setAttribute('src', 'https://sub1.test1.example.com');
        iframe.onload = resolve;
        content.document.body.appendChild(iframe);
      });

      // Now that the https subdomain has loaded, we want to make sure that the
      // permissions are also avaliable for its parent domain, https://example.com!
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "perm1"),
         Services.perms.ALLOW_ACTION, "perm1-4");
      is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                       "newperm1"),
         Services.perms.ALLOW_ACTION, "newperm1-4");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "perm2"),
         Services.perms.UNKNOWN_ACTION, "perm2-4");
      is(Services.perms.testPermission(Services.io.newURI("http://foo.bar.example.com"),
                                       "newperm2"),
         Services.perms.UNKNOWN_ACTION, "newperm2-4");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "perm3"),
         Services.perms.ALLOW_ACTION, "perm3-4");
      is(Services.perms.testPermission(Services.io.newURI("about:home"),
                                       "newperm3"),
         Services.perms.ALLOW_ACTION, "newperm3-4");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "perm4"),
         Services.perms.ALLOW_ACTION, "perm4-4");
      is(Services.perms.testPermission(Services.io.newURI("https://example.com"),
                                       "newperm4"),
         Services.perms.ALLOW_ACTION, "newperm4-4");
      is(Services.perms.testPermission(Services.io.newURI("https://somerandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "document-4");
      is(Services.perms.testPermission(Services.io.newURI("https://someotherrandomwebsite.com"),
                                       "document"),
         Services.perms.ALLOW_ACTION, "otherdocument-4");
    });
  });
});
