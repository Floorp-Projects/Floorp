const FOLDER = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

add_task(async function() {
  await BrowserTestUtils.withNewTab("about:robots", async browser => {
    is(browser.currentURI.spec, "about:robots");

    let uri = `view-source:${FOLDER}/dummy_page.html`;
    BrowserTestUtils.loadURI(browser, uri);
    await BrowserTestUtils.browserLoaded(browser);

    is(browser.currentURI.spec, uri);
  });
});
