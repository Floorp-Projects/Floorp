add_task(async function setup() {
  Services.prefs.setBoolPref("security.data_uri.unique_opaque_origin", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("security.data_uri.unique_opaque_origin");
  });
});

add_task(async function test_dataURI_unique_opaque_origin() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  let pagePrincipal = browser.contentPrincipal;
  info("pagePrincial " + pagePrincipal.origin);

  browser.loadURIWithFlags("data:text/html,hi", 0, null, null, null);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, { principal: pagePrincipal }, async function(args) {
    info("data URI principal: " + content.document.nodePrincipal.origin);
    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "data: URI should have NullPrincipal.");
    Assert.ok(!content.document.nodePrincipal.equals(args.principal),
              "data: URI should have unique opaque origin.");
  });

  gBrowser.removeTab(tab);
});
