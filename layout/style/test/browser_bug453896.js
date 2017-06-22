add_task(async function() {
  let uri = getRootDirectory(gTestPath) + "bug453896_iframe.html";

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: uri
  }, function(browser) {
    return ContentTask.spawn(browser, null, async function() {
      var fake_window = { ok: ok };
      content.wrappedJSObject.run(fake_window);
    });
  });
});
