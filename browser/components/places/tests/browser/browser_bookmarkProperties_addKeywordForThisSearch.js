"use strict"

const TEST_URL = "http://mochi.test:8888/browser/browser/components/places/tests/browser/keyword_form.html";

add_task(function* () {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, function* (browser) {
    // We must wait for the context menu code to build metadata.
    yield openContextMenuForContentSelector(browser, 'form > input[name="search"]');

    yield withBookmarksDialog(true, AddKeywordForSearchField, function* (dialogWin) {
      let acceptBtn = dialogWin.document.documentElement.getButton("accept");
      ok(acceptBtn.disabled, "Accept button is disabled");

      let promiseKeywordNotification = promiseBookmarksNotification(
        "onItemChanged", (itemId, prop, isAnno, val) => prop == "keyword" && val =="kw");

      fillBookmarkTextField("editBMPanel_keywordField", "kw", dialogWin);

      ok(!acceptBtn.disabled, "Accept button is enabled");

      // The dialog is instant apply.
      yield promiseKeywordNotification;

      // After the notification, the keywords cache will update asynchronously.
      info("Check the keyword entry has been created");
      let entry;
      yield waitForCondition(function* () {
        entry = yield PlacesUtils.keywords.fetch("kw");
        return !!entry;
      }, "Unable to find the expected keyword");
      is(entry.keyword, "kw", "keyword is correct");
      is(entry.url.href, TEST_URL, "URL is correct");
      is(entry.postData, "accenti%3D%E0%E8%EC%F2%F9&search%3D%25s", "POST data is correct");

      info("Check the charset has been saved");
      let charset = yield PlacesUtils.getCharsetForURI(NetUtil.newURI(TEST_URL));
      is(charset, "windows-1252", "charset is correct");

      // Now check getShortcutOrURI.
      let data = yield getShortcutOrURIAndPostData("kw test");
      is(getPostDataString(data.postData), "accenti=\u00E0\u00E8\u00EC\u00F2\u00F9&search=test", "getShortcutOrURI POST data is correct");
      is(data.url, TEST_URL, "getShortcutOrURI URL is correct");
    });
  });
});

function getPostDataString(stream) {
  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(stream);
  return sis.read(stream.available()).split("\n").pop();
}
