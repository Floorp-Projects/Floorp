/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
Test that after using document.write(...), refreshing the document and calling write again,
resulting document.URL is identical to the original URL.

This testcase is aimed at preventing bug 619092
*/
var testURL =
  "http://mochi.test:8888/browser/dom/html/test/file_refresh_after_document_write.html";
let aTab, aBrowser;

function test() {
  waitForExplicitFinish();

  aTab = BrowserTestUtils.addTab(gBrowser, testURL);
  aBrowser = gBrowser.getBrowserForTab(aTab);
  BrowserTestUtils.browserLoaded(aBrowser)
    .then(() => {
      is(
        aBrowser.currentURI.spec,
        testURL,
        "Make sure we start at the correct URL"
      );

      ContentTask.spawn(aBrowser, null, () => {
        // test_btn calls document.write() then reloads the document
        let test_btn = content.document.getElementById("test_btn");

        addEventListener(
          "load",
          () => {
            test_btn.click();
          },
          { once: true, capture: true }
        );

        test_btn.click();
      });

      return BrowserTestUtils.browserLoaded(aBrowser);
    })
    .then(() => {
      return ContentTask.spawn(aBrowser, null, () => content.document.URL);
    })
    .then(url => {
      is(url, testURL, "Document URL should be identical after reload");
      gBrowser.removeTab(aTab);
      finish();
    });
}
