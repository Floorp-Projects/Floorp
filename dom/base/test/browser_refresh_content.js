/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Ensures resources can still be fetched without validation
 */

const CONTENT_URL =
  "http://www.example.com/browser/dom/base/test/file_browser_refresh_content.html";

async function run_test_browser_refresh(forceRevalidate) {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: CONTENT_URL,
    waitForLoad: true,
    /* Ensures each run is started with a fresh state */
    forceNewProcess: true,
  });

  let originalAttributes = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      let result = content.document.getElementById("result");
      await ContentTaskUtils.waitForCondition(() => {
        return (
          result.getAttribute("imageDataURL") &&
          result.getAttribute("iframeContent") &&
          result.getAttribute("expiredResourceCacheControl")
        );
      });
      return {
        imageDataURL: result.getAttribute("imageDataURL"),
        iframeContent: result.getAttribute("iframeContent"),
        expiredResourceCacheControl: result.getAttribute(
          "expiredResourceCacheControl"
        ),
      };
    }
  );

  let imageDataURL = originalAttributes.imageDataURL;
  let expiredResourceCacheControl =
    originalAttributes.expiredResourceCacheControl;

  is(
    originalAttributes.iframeContent,
    "first load",
    "Iframe is loaded with the initial content"
  );

  tab.linkedBrowser.reload();

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let attributes = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      let result = content.document.getElementById("result");

      await ContentTaskUtils.waitForCondition(() => {
        return (
          result.getAttribute("imageDataURL") &&
          result.getAttribute("expiredResourceCacheControl") &&
          result.getAttribute("nonCacheableResourceCompleted")
        );
      });

      return {
        iframeContent: result.getAttribute("iframeContent"),
        imageDataURL: result.getAttribute("imageDataURL"),
        expiredResourceCacheControl: result.getAttribute(
          "expiredResourceCacheControl"
        ),
        nonCacheableResourceCompleted: result.getAttribute(
          "nonCacheableResourceCompleted"
        ),
      };
    }
  );

  is(
    attributes.iframeContent,
    "second load",
    "Iframe should always be revalidated"
  );

  if (!forceRevalidate) {
    Assert.strictEqual(
      attributes.imageDataURL,
      imageDataURL,
      "Image should use cache"
    );
  } else {
    Assert.notStrictEqual(
      attributes.imageDataURL,
      imageDataURL,
      "Image should be revalidated"
    );
  }

  if (!forceRevalidate) {
    Assert.strictEqual(
      attributes.expiredResourceCacheControl,
      expiredResourceCacheControl,
      "max-age shouldn't be changed after reload because it didn't revalidate"
    );
  } else {
    Assert.notStrictEqual(
      attributes.expiredResourceCacheControl,
      expiredResourceCacheControl,
      "max-age should be changed after reload because it got revalidated"
    );
  }

  is(
    attributes.nonCacheableResourceCompleted,
    "true",
    "Non cacheable resource should still be loaded"
  );

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_browser_refresh() {
  Services.prefs.setBoolPref(
    "browser.soft_reload.only_force_validate_top_level_document",
    true
  );
  await run_test_browser_refresh(false);
  Services.prefs.setBoolPref(
    "browser.soft_reload.only_force_validate_top_level_document",
    false
  );
  await run_test_browser_refresh(true);
});
