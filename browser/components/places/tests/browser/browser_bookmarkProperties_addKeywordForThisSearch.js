"use strict";

const TEST_URL = "http://mochi.test:8888/browser/browser/components/places/tests/browser/keyword_form.html";

function closeHandler(dialogWin) {
  let savedItemId = dialogWin.gEditItemOverlay.itemId;
  return PlacesTestUtils.waitForNotification("onItemRemoved",
                                             itemId => itemId === savedItemId);
}

add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, async function(browser) {
    // We must wait for the context menu code to build metadata.
    await openContextMenuForContentSelector(browser, '#form1 > input[name="search"]');

    await withBookmarksDialog(true, AddKeywordForSearchField, async function(dialogWin) {
      let acceptBtn = dialogWin.document.documentElement.getButton("accept");
      ok(acceptBtn.disabled, "Accept button is disabled");

      let promiseKeywordNotification = PlacesTestUtils.waitForNotification(
        "onItemChanged", (itemId, prop, isAnno, val) => prop == "keyword" && val == "kw");

      fillBookmarkTextField("editBMPanel_keywordField", "kw", dialogWin);

      ok(!acceptBtn.disabled, "Accept button is enabled");

      // The dialog is instant apply.
      await promiseKeywordNotification;

      // After the notification, the keywords cache will update asynchronously.
      info("Check the keyword entry has been created");
      let entry;
      await waitForCondition(async function() {
        entry = await PlacesUtils.keywords.fetch("kw");
        return !!entry;
      }, "Unable to find the expected keyword");
      is(entry.keyword, "kw", "keyword is correct");
      is(entry.url.href, TEST_URL, "URL is correct");
      is(entry.postData, "accenti%3D%E0%E8%EC%F2%F9&search%3D%25s", "POST data is correct");

      info("Check the charset has been saved");
      let charset = await PlacesUtils.getCharsetForURI(NetUtil.newURI(TEST_URL));
      is(charset, "windows-1252", "charset is correct");

      // Now check getShortcutOrURI.
      let data = await getShortcutOrURIAndPostData("kw test");
      is(getPostDataString(data.postData), "accenti=\u00E0\u00E8\u00EC\u00F2\u00F9&search=test", "getShortcutOrURI POST data is correct");
      is(data.url, TEST_URL, "getShortcutOrURI URL is correct");
    }, closeHandler);
  });
});

add_task(async function reopen_same_field() {
  await PlacesUtils.keywords.insert({
    url: TEST_URL,
    keyword: "kw",
    postData: "accenti%3D%E0%E8%EC%F2%F9&search%3D%25s"
  });
  registerCleanupFunction(async function() {
    await PlacesUtils.keywords.remove("kw");
  });
  // Reopening on the same input field should show the existing keyword.
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, async function(browser) {
    // We must wait for the context menu code to build metadata.
    await openContextMenuForContentSelector(browser, '#form1 > input[name="search"]');

    await withBookmarksDialog(true, AddKeywordForSearchField, async function(dialogWin) {
      let acceptBtn = dialogWin.document.documentElement.getButton("accept");
      ok(acceptBtn.disabled, "Accept button is disabled");

      let elt = dialogWin.document.getElementById("editBMPanel_keywordField");
      await BrowserTestUtils.waitForCondition(() => elt.value == "kw", "Keyword should be the previous value");
    }, closeHandler);
  });
});

add_task(async function open_other_field() {
  await PlacesUtils.keywords.insert({
    url: TEST_URL,
    keyword: "kw2",
    postData: "search%3D%25s"
  });
  registerCleanupFunction(async function() {
    await PlacesUtils.keywords.remove("kw2");
  });
  // Reopening on another field of the same page that has different postData
  // should not show the existing keyword.
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, async function(browser) {
    // We must wait for the context menu code to build metadata.
    await openContextMenuForContentSelector(browser, '#form2 > input[name="search"]');

    await withBookmarksDialog(true, AddKeywordForSearchField, function(dialogWin) {
      let acceptBtn = dialogWin.document.documentElement.getButton("accept");
      ok(acceptBtn.disabled, "Accept button is disabled");

      let elt = dialogWin.document.getElementById("editBMPanel_keywordField");
      is(elt.value, "");
    }, closeHandler);
  });
});

function getPostDataString(stream) {
  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
              .createInstance(Ci.nsIScriptableInputStream);
  sis.init(stream);
  return sis.read(stream.available()).split("\n").pop();
}
