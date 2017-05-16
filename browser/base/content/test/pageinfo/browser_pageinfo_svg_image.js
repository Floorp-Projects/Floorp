function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gBrowser.selectedBrowser.addEventListener("load", function() {
    var pageInfo = BrowserPageInfo(gBrowser.selectedBrowser.currentURI.spec,
                                   "mediaTab");

    pageInfo.addEventListener("load", function() {
      pageInfo.onFinished.push(function() {
        executeSoon(function() {
          var imageTree = pageInfo.document.getElementById("imagetree");
          var imageRowsNum = imageTree.view.rowCount;

          ok(imageTree, "Image tree is null (media tab is broken)");

          is(imageRowsNum, 1, "should have one image");

          // Only bother running this if we've got the right number of rows.
          if (imageRowsNum == 1) {
            is(imageTree.view.getCellText(0, imageTree.columns[0]),
               "https://example.com/browser/browser/base/content/test/pageinfo/title_test.svg",
               "The URL should be the svg image.");
          }

          pageInfo.close();
          gBrowser.removeCurrentTab();
          finish();
        });
      });
    }, {capture: true, once: true});
  }, {capture: true, once: true});

  content.location =
    "https://example.com/browser/browser/base/content/test/pageinfo/svg_image.html";
}
