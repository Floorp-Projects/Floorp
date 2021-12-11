var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);

/**
 * Wait for url's page info (non-null description and preview url) to be set.
 * Because there is debounce logic in ContentLinkHandler.jsm to only make one
 * single SQL update, we have to wait for some time before checking that the page
 * info was stored.
 */
async function waitForPageInfo(url) {
  let pageInfo;
  await BrowserTestUtils.waitForCondition(async () => {
    pageInfo = await PlacesUtils.history.fetch(url, { includeMeta: true });
    return pageInfo && pageInfo.description && pageInfo.previewImageURL;
  });
  return pageInfo;
}
