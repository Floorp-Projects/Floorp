add_task(function* () {
  let newWindow = yield BrowserTestUtils.openNewBrowserWindow();

  let resizedPromise = BrowserTestUtils.waitForEvent(newWindow, "resize");
  newWindow.resizeTo(300, 300);
  yield resizedPromise;

  yield BrowserTestUtils.openNewForegroundTab(newWindow.gBrowser, "about:home");

  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    Assert.equal(content.document.body.getAttribute("narrow"), "true", "narrow mode");
  });

  resizedPromise = BrowserTestUtils.waitForContentEvent(newWindow.gBrowser.selectedBrowser, "resize");


  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    content.window.resizeTo(800, 800);
  });

  yield resizedPromise;

  yield ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, function* () {
    Assert.equal(content.document.body.hasAttribute("narrow"), false, "non-narrow mode");
  });

  yield BrowserTestUtils.closeWindow(newWindow);
});
