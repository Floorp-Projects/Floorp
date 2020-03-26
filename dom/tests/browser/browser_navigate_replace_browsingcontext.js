add_task(async function parent_to_remote() {
  await BrowserTestUtils.withNewTab("about:mozilla", async browser => {
    let originalBC = browser.browsingContext;

    BrowserTestUtils.loadURI(browser, "https://example.com/");
    await BrowserTestUtils.browserLoaded(browser);
    let newBC = browser.browsingContext;

    isnot(originalBC.id, newBC.id,
          "Should have replaced the BrowsingContext");
  });
});

add_task(async function remote_to_parent() {
  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    let originalBC = browser.browsingContext;

    BrowserTestUtils.loadURI(browser, "about:mozilla");
    await BrowserTestUtils.browserLoaded(browser);
    let newBC = browser.browsingContext;

    isnot(originalBC.id, newBC.id,
          "Should have replaced the BrowsingContext");
  });
});
