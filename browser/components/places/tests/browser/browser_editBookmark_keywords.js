"use strict";

const TEST_URL = "about:blank";

add_task(async function () {
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
    const promise = PlacesTestUtils.waitForNotification(
      "bookmark-keyword-changed"
    );
    keywordField.focus();
    keywordField.value = "kw";
    EventUtils.sendString(i.toString(), library);
    keywordField.blur();
    const events = await promise;
    is(events.length, 1, "Number of events fired is correct");
    const keyword = events[0].keyword;
    is(keyword, `kw${i}`, "The new keyword value is correct");
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
