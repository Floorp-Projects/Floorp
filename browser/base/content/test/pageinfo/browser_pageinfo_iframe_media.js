/* Check proper media data retrieval in case of iframe */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

add_task(async function test_all_images_mentioned() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "iframes.html",
    async function() {
      let pageInfo = BrowserPageInfo(
        gBrowser.selectedBrowser.currentURI.spec,
        "mediaTab"
      );
      await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

      let imageTree = pageInfo.document.getElementById("imagetree");
      let imageRowsNum = imageTree.view.rowCount;

      ok(imageTree, "Image tree is null (media tab is broken)");
      ok(
        imageRowsNum == 2,
        "Number of media items listed: " + imageRowsNum + ", should be 2"
      );

      pageInfo.close();
    }
  );
});
