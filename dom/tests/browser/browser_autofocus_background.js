add_task(async function () {
  const URL =
    "data:text/html,<!DOCTYPE html><html><body><input autofocus id='target'></body></html>";
  const foregroundTab = gBrowser.selectedTab;
  const backgroundTab = BrowserTestUtils.addTab(gBrowser);

  // Ensure tab is still in the foreground.
  is(
    gBrowser.selectedTab,
    foregroundTab,
    "foregroundTab should still be selected"
  );

  // Load the second tab in the background.
  const loadedPromise = BrowserTestUtils.browserLoaded(
    backgroundTab.linkedBrowser,
    /* includesubframes */ false,
    URL
  );
  BrowserTestUtils.startLoadingURIString(backgroundTab.linkedBrowser, URL);
  await loadedPromise;

  // Get active element in the tab.
  let tagName = await SpecialPowers.spawn(
    backgroundTab.linkedBrowser,
    [],
    async function () {
      // Spec asks us to flush autofocus candidates in the
      // `update-the-rendering` step, so we need to wait
      // for a rAF to ensure autofocus candidates are
      // flushed.
      await new Promise(r => {
        content.requestAnimationFrame(r);
      });
      return content.document.activeElement.tagName;
    }
  );

  is(
    tagName,
    "INPUT",
    "The background tab's focused element should be the <input>"
  );

  is(
    gBrowser.selectedTab,
    foregroundTab,
    "foregroundTab tab should still be selected, shouldn't cause a tab switch"
  );

  is(
    document.activeElement,
    foregroundTab.linkedBrowser,
    "The background tab's focused element should not cause the tab to be selected"
  );

  // Cleaning up.
  BrowserTestUtils.removeTab(backgroundTab);
});
