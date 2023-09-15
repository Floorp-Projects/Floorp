add_task(async () => {
  let testPath = getRootDirectory(gTestPath);

  // The default favicon would interfere with this test.
  Services.prefs.setBoolPref("browser.chrome.guess_favicon", false);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.chrome.guess_favicon", true);
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      const expectedIcon = testPath + "file_generic_favicon.ico";
      let faviconPromise = waitForLinkAvailable(browser);

      BrowserTestUtils.startLoadingURIString(
        browser,
        testPath + "file_with_favicon.html"
      );
      await BrowserTestUtils.browserLoaded(browser);

      let iconURI = await faviconPromise;
      is(iconURI, expectedIcon, "Got correct icon.");

      BrowserTestUtils.startLoadingURIString(browser, testPath + "blank.html");
      await BrowserTestUtils.browserLoaded(browser);

      is(browser.mIconURL, null, "Should have blanked the icon.");
      is(
        gBrowser.getTabForBrowser(browser).getAttribute("image"),
        "",
        "Should have blanked the tab icon."
      );
    }
  );
});
