add_task(async function test() {
  let testPath = getRootDirectory(gTestPath);

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
    async function(tabBrowser) {
      const URI = testPath + "file_with_favicon.html";
      const expectedIcon = testPath + "file_generic_favicon.ico";
      let faviconPromise = waitForLinkAvailable(tabBrowser);

      BrowserTestUtils.loadURI(tabBrowser, URI);

      let iconURI = await faviconPromise;
      is(iconURI, expectedIcon, "Correct icon before pushState.");

      faviconPromise = waitForLinkAvailable(tabBrowser);

      await ContentTask.spawn(tabBrowser, null, function() {
        content.history.pushState("page2", "page2", "page2");
      });

      // We've navigated and shouldn't get a call to onLinkIconAvailable.
      TestUtils.executeSoon(() => {
        faviconPromise.cancel();
      });

      try {
        await faviconPromise;
        ok(false, "Should not have seen a new icon load.");
      } catch (e) {
        ok(true, "Should have been able to cancel the promise.");
      }
    });
});
