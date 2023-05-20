add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (newBrowser) {
      await SpecialPowers.spawn(newBrowser, [], async function () {
        var prin = content.document.nodePrincipal;
        Assert.notEqual(prin, null, "Loaded principal must not be null");
        Assert.notEqual(
          prin,
          undefined,
          "Loaded principal must not be undefined"
        );

        Assert.equal(
          prin.isSystemPrincipal,
          false,
          "Loaded principal must not be system"
        );
      });
    }
  );
});
