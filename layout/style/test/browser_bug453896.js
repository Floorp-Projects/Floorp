add_task(function* () {
  let uri = getRootDirectory(gTestPath) + "bug453896_iframe.html";

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: uri
  }, function*(browser) {
    return ContentTask.spawn(browser, null, function* () {
      var fake_window = { ok: ok };
      content.wrappedJSObject.run(fake_window);
    });
  });
});
