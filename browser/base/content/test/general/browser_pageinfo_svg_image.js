function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);
    var pageInfo = BrowserPageInfo(gBrowser.selectedBrowser.currentURI.spec,
                                   "mediaTab");

    pageInfo.addEventListener("load", function loadListener2() {
      pageInfo.removeEventListener("load", loadListener2, true);
      pageInfo.onFinished.push(function() {
        executeSoon(function() {
          var imageTree = pageInfo.document.getElementById("imagetree");
          var imageRowsNum = imageTree.view.rowCount;

          ok(imageTree, "Image tree is null (media tab is broken)");

          is(imageRowsNum, 1, "should have one image");

          // Only bother running this if we've got the right number of rows.
          if (imageRowsNum == 1) {
            is(imageTree.view.getCellText(0, imageTree.columns[0]),
               "https://example.com/browser/browser/base/content/test/general/title_test.svg",
               "The URL should be the svg image.");
          }

          pageInfo.close();
          gBrowser.removeCurrentTab();
          finish();
        });
      });
    }, true);
  }, true);

  content.location =
    "https://example.com/browser/browser/base/content/test/general/svg_image.html";
}
