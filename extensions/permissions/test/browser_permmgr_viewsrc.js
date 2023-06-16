add_task(async function () {
  // Add a permission for example.com, start a new content process, and make
  // sure that the permission has been sent down.
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.com"
    );
  Services.perms.addFromPrincipal(
    principal,
    "viewsourceTestingPerm",
    Services.perms.ALLOW_ACTION
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "view-source:https://example.com",
    /* waitForLoad */ true,
    /* waitForStateStop */ false,
    /* forceNewProcess */ true
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [principal], async function (p) {
    is(
      Services.perms.testPermissionFromPrincipal(p, "viewsourceTestingPerm"),
      Services.perms.ALLOW_ACTION
    );
  });
  BrowserTestUtils.removeTab(tab);
});
