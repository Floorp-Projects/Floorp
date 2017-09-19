Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

/**
 * Wait for url's page info (non-null description and preview url) to be set.
 */
async function waitForPageInfo(url) {
  let pageInfo;
  await BrowserTestUtils.waitForCondition(async () => {
    pageInfo = await PlacesUtils.history.fetch(url, {"includeMeta": true});
    return pageInfo && pageInfo.description && pageInfo.previewImageURL;
  });
  return pageInfo;
}
