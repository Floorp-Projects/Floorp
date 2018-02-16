/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
Test that after using document.write(...), refreshing the document and calling write again,
resulting document.URL does not contain 'wyciwyg' schema
and instead is identical to the original URL.

This testcase is aimed at preventing bug 619092
*/
var testURL = "http://mochi.test:8888/browser/dom/html/test/file_refresh_wyciwyg_url.html";
let aTab, aBrowser, test_btn;

function test(){
    waitForExplicitFinish();

    aTab = BrowserTestUtils.addTab(gBrowser, testURL);
    aBrowser = gBrowser.getBrowserForTab(aTab);
    BrowserTestUtils.browserLoaded(aBrowser).then(() => {
        is(aBrowser.contentDocument.URL, testURL, "Make sure we start at the correct URL");

        // test_btn calls document.write() then reloads the document
        test_btn = aBrowser.contentDocument.getElementById("test_btn");
        test_btn.click();
        return BrowserTestUtils.browserLoaded(aBrowser);
    }).then(() => {
        test_btn.click();
        is(aBrowser.contentDocument.URL, testURL, "Document URL should be identical after reload");
        gBrowser.removeTab(aTab);
        finish();
    });
}
