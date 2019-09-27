add_task(async function() {
  // Add a permission for example.com, start a new content process, and make
  // sure that the permission has been sent down.
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://example.com"
  );
  Services.perms.addFromPrincipal(
    principal,
    "viewsourceTestingPerm",
    Services.perms.ALLOW_ACTION
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "view-source:http://example.com",
    /* waitForLoad */ true,
    /* waitForStateStop */ false,
    /* forceNewProcess */ true
  );
  await ContentTask.spawn(tab.linkedBrowser, principal, async function(p) {
    is(
      Services.perms.testPermissionFromPrincipal(p, "viewsourceTestingPerm"),
      Services.perms.ALLOW_ACTION
    );
  });
  BrowserTestUtils.removeTab(tab);
});
