add_task(async function test() {
  const uris = [undefined, "about:blank"];

  function checkContentProcess(newBrowser, uri) {
    return ContentTask.spawn(newBrowser, [uri], async function (uri) {
      var prin = content.document.nodePrincipal;
      Assert.notEqual(
        prin,
        null,
        "Loaded principal must not be null when adding " + uri
      );
      Assert.notEqual(
        prin,
        undefined,
        "Loaded principal must not be undefined when loading " + uri
      );

      Assert.equal(
        prin.isSystemPrincipal,
        false,
        "Loaded principal must not be system when loading " + uri
      );
    });
  }

  for (var uri of uris) {
    await BrowserTestUtils.withNewTab(
      { gBrowser },
      async function (newBrowser) {
        let loadedPromise = BrowserTestUtils.browserLoaded(newBrowser);
        BrowserTestUtils.startLoadingURIString(newBrowser, uri);

        var prin = newBrowser.contentPrincipal;
        isnot(
          prin,
          null,
          "Forced principal must not be null when loading " + uri
        );
        isnot(
          prin,
          undefined,
          "Forced principal must not be undefined when loading " + uri
        );
        is(
          prin.isSystemPrincipal,
          false,
          "Forced principal must not be system when loading " + uri
        );

        // Belt-and-suspenders e10s check: make sure that the same checks hold
        // true in the content process.
        await checkContentProcess(newBrowser, uri);

        await loadedPromise;

        prin = newBrowser.contentPrincipal;
        isnot(
          prin,
          null,
          "Loaded principal must not be null when adding " + uri
        );
        isnot(
          prin,
          undefined,
          "Loaded principal must not be undefined when loading " + uri
        );
        is(
          prin.isSystemPrincipal,
          false,
          "Loaded principal must not be system when loading " + uri
        );

        // Belt-and-suspenders e10s check: make sure that the same checks hold
        // true in the content process.
        await checkContentProcess(newBrowser, uri);
      }
    );
  }
});
