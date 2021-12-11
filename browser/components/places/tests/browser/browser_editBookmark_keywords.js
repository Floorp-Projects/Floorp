"use strict";

const TEST_URL = "about:blank";

add_task(async function() {
  function promiseOnItemChanged() {
    return new Promise(resolve => {
      PlacesUtils.bookmarks.addObserver({
        onItemChanged(id, property, isAnno, value) {
          PlacesUtils.bookmarks.removeObserver(this);
          resolve({ property, value });
        },
        QueryInterface: ChromeUtils.generateQI(["nsINavBookmarkObserver"]),
      });
    });
  }

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  let library = await promiseLibrary("UnfiledBookmarks");
  registerCleanupFunction(async () => {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
    await BrowserTestUtils.removeTab(tab);
  });

  let keywordField = library.document.getElementById(
    "editBMPanel_keywordField"
  );

  for (let i = 0; i < 2; ++i) {
    let bm = await PlacesUtils.bookmarks.insert({
      url: `http://www.test${i}.me/`,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });

    let node = library.ContentTree.view.view.nodeForTreeIndex(i);
    is(node.bookmarkGuid, bm.guid, "Found the expected bookmark");
    // Select the bookmark.
    library.ContentTree.view.selectNode(node);
    synthesizeClickOnSelectedTreeCell(library.ContentTree.view);

    is(
      library.document.getElementById("editBMPanel_keywordField").value,
      "",
      "The keyword field should be empty"
    );
    info("Add a keyword to the bookmark");
    let promise = promiseOnItemChanged();
    keywordField.focus();
    keywordField.value = "kw";
    EventUtils.sendString(i.toString(), library);
    keywordField.blur();
    let { property, value } = await promise;
    is(property, "keyword", "The keyword should have been changed");

    is(value, `kw${i}`, "The new keyword value is correct");
  }

  for (let i = 0; i < 2; ++i) {
    let entry = await PlacesUtils.keywords.fetch({
      url: `http://www.test${i}.me/`,
    });
    is(
      entry.keyword,
      `kw${i}`,
      `The keyword for http://www.test${i}.me/ is correct`
    );
  }
});
