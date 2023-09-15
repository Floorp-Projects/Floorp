add_task(async function test_dataURI_unique_opaque_origin() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com");
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  let pagePrincipal = browser.contentPrincipal;
  info("pagePrincial " + pagePrincipal.origin);

  BrowserTestUtils.startLoadingURIString(browser, "data:text/html,hi");
  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(
    browser,
    [{ principal: pagePrincipal }],
    async function (args) {
      info("data URI principal: " + content.document.nodePrincipal.origin);
      Assert.ok(
        content.document.nodePrincipal.isNullPrincipal,
        "data: URI should have NullPrincipal."
      );
      Assert.ok(
        !content.document.nodePrincipal.equals(args.principal),
        "data: URI should have unique opaque origin."
      );
    }
  );

  gBrowser.removeTab(tab);
});
