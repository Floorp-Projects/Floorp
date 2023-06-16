add_task(async function test_multiple_pushState() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://example.org/browser/docshell/test/browser/file_multiple_pushState.html",
    },
    async function (browser) {
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      const kExpected = "http://example.org/bar/ABC/DEF?key=baz";

      let contentLocation = await SpecialPowers.spawn(
        browser,
        [],
        async function () {
          return content.document.location.href;
        }
      );

      is(contentLocation, kExpected);
      is(browser.documentURI.spec, kExpected);
    }
  );
});
