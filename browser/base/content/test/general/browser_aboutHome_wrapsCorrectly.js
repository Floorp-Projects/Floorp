add_task(async function() {
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  let resizedPromise = BrowserTestUtils.waitForEvent(newWindow, "resize");
  newWindow.resizeTo(300, 300);
  await resizedPromise;

  await BrowserTestUtils.openNewForegroundTab(newWindow.gBrowser, "about:home");

  await ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, async function() {
    Assert.equal(content.document.body.getAttribute("narrow"), "true", "narrow mode");
  });

  resizedPromise = BrowserTestUtils.waitForContentEvent(newWindow.gBrowser.selectedBrowser, "resize");


  await ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, async function() {
    content.window.resizeTo(800, 800);
  });

  await resizedPromise;

  await ContentTask.spawn(newWindow.gBrowser.selectedBrowser, {}, async function() {
    Assert.equal(content.document.body.hasAttribute("narrow"), false, "non-narrow mode");
  });

  await BrowserTestUtils.closeWindow(newWindow);
});
