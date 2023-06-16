add_task(async function () {
  let uri = getRootDirectory(gTestPath) + "bug453896_iframe.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: uri,
    },
    function (browser) {
      return SpecialPowers.spawn(browser, [], async function () {
        var fake_window = { ok: ok };
        content.wrappedJSObject.run(fake_window);
      });
    }
  );
});
