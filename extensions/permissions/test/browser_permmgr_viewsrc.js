add_task(async function() {
  // Add a permission for example.com, start a new content process, and make
  // sure that the permission has been sent down.
  Services.perms.add(Services.io.newURI("http://example.com"),
                     "viewsourceTestingPerm",
                     Services.perms.ALLOW_ACTION);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser,
                                                        "view-source:http://example.com",
                                                        /* waitForLoad */ true,
                                                        /* waitForStateStop */ false,
                                                        /* forceNewProcess */ true);
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    is(Services.perms.testPermission(Services.io.newURI("http://example.com"),
                                     "viewsourceTestingPerm"),
       Services.perms.ALLOW_ACTION);
  });
  BrowserTestUtils.removeTab(tab);
});
