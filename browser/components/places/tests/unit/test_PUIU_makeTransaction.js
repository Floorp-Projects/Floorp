/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function waitForBookmarkNotification(aNotification, aCallback, aProperty) {
  PlacesUtils.bookmarks.addObserver({
    validate(aMethodName, aData) {
      if (aMethodName == aNotification &&
          (!aProperty || aProperty == aData.property)) {
        PlacesUtils.bookmarks.removeObserver(this);
        aCallback(aData);
      }
    },

    // nsINavBookmarkObserver
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver]),
    onBeginUpdateBatch: function onBeginUpdateBatch() {
      return this.validate("onBeginUpdateBatch", arguments);
    },
    onEndUpdateBatch: function onEndUpdateBatch() {
      return this.validate("onEndUpdateBatch", arguments);
    },
    onItemAdded: function onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                      aURI, aTitle) {
      return this.validate("onItemAdded", { id: aItemId,
                                                    index: aIndex,
                                                    type: aItemType,
                                                    url: aURI ? aURI.spec : null,
                                                    title: aTitle });
    },
    onItemRemoved: function onItemRemoved() {
      return this.validate("onItemRemoved", arguments);
    },
    onItemChanged: function onItemChanged(id, property, aIsAnno,
                                          aNewValue, aLastModified, type) {
      return this.validate("onItemChanged",
                           { id,
                             get index() {
                               return PlacesUtils.bookmarks.getItemIndex(this.id);
                             },
                             type,
                             property,
                             get url() {
                               return type == PlacesUtils.bookmarks.TYPE_BOOKMARK ?
                                      PlacesUtils.bookmarks.getBookmarkURI(this.id).spec :
                                      null;
                             },
                             get title() {
                               return PlacesUtils.bookmarks.getItemTitle(this.id);
                             },
                           });
    },
    onItemVisited: function onItemVisited() {
      return this.validate("onItemVisited", arguments);
    },
    onItemMoved: function onItemMoved(aItemId, aOldParentId, aOldIndex,
                                      aNewParentId, aNewIndex, aItemType) {
      this.validate("onItemMoved", { id: aItemId,
                                             index: aNewIndex,
                                             type: aItemType });
    }
  });
}

function wrapNodeByIdAndParent(aItemId, aParentId) {
  let wrappedNode;
  let root = PlacesUtils.getFolderContents(aParentId, false, false).root;
  for (let i = 0; i < root.childCount; ++i) {
    let node = root.getChild(i);
    if (node.itemId == aItemId) {
      let type;
      if (PlacesUtils.nodeIsContainer(node)) {
        type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;
      } else if (PlacesUtils.nodeIsURI(node)) {
        type = PlacesUtils.TYPE_X_MOZ_PLACE;
      } else if (PlacesUtils.nodeIsSeparator(node)) {
        type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
      } else {
        do_throw("Unknown node type");
      }
      wrappedNode = PlacesUtils.wrapNode(node, type);
    }
  }
  root.containerOpen = false;
  return JSON.parse(wrappedNode);
}

add_test(function test_text_paste() {
  const TEST_URL = "http://places.moz.org/";
  const TEST_TITLE = "Places bookmark";

  waitForBookmarkNotification("onItemAdded", function(aData) {
    Assert.equal(aData.title, TEST_TITLE);
    Assert.equal(aData.url, TEST_URL);
    Assert.equal(aData.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    Assert.equal(aData.index, 0);
    run_next_test();
  });

  let txn = PlacesUIUtils.makeTransaction(
    { title: TEST_TITLE, uri: TEST_URL },
    PlacesUtils.TYPE_X_MOZ_URL,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.bookmarks.DEFAULT_INDEX,
    true // Unused for text.
  );
  PlacesUtils.transactionManager.doTransaction(txn);
});

add_test(function test_container() {
  const TEST_TITLE = "Places folder";

  waitForBookmarkNotification("onItemChanged", function(aChangedData) {
    Assert.equal(aChangedData.title, TEST_TITLE);
    Assert.equal(aChangedData.type, PlacesUtils.bookmarks.TYPE_FOLDER);
    Assert.equal(aChangedData.index, 1);

    waitForBookmarkNotification("onItemAdded", function(aAddedData) {
      Assert.equal(aAddedData.title, TEST_TITLE);
      Assert.equal(aAddedData.type, PlacesUtils.bookmarks.TYPE_FOLDER);
      Assert.equal(aAddedData.index, 2);
      let id = aAddedData.id;

      waitForBookmarkNotification("onItemMoved", function(aMovedData) {
        Assert.equal(aMovedData.id, id);
        Assert.equal(aMovedData.type, PlacesUtils.bookmarks.TYPE_FOLDER);
        Assert.equal(aMovedData.index, 1);

        run_next_test();
      });

      let txn = PlacesUIUtils.makeTransaction(
        wrapNodeByIdAndParent(aAddedData.id, PlacesUtils.unfiledBookmarksFolderId),
        0, // Unused for real nodes.
        PlacesUtils.unfiledBookmarksFolderId,
        1, // Move to position 1.
        false
      );
      PlacesUtils.transactionManager.doTransaction(txn);
    });

    try {
    let txn = PlacesUIUtils.makeTransaction(
      wrapNodeByIdAndParent(aChangedData.id, PlacesUtils.unfiledBookmarksFolderId),
      0, // Unused for real nodes.
      PlacesUtils.unfiledBookmarksFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      true
    );
    PlacesUtils.transactionManager.doTransaction(txn);
    } catch (ex) {
      do_throw(ex);
    }
  }, "random-anno");

  let id = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                              TEST_TITLE,
                                              PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.annotations.setItemAnnotation(id, PlacesUIUtils.DESCRIPTION_ANNO,
                                            "description", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setItemAnnotation(id, "random-anno",
                                            "random-value", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
});


add_test(function test_separator() {
  waitForBookmarkNotification("onItemChanged", function(aChangedData) {
    Assert.equal(aChangedData.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
    Assert.equal(aChangedData.index, 3);

    waitForBookmarkNotification("onItemAdded", function(aAddedData) {
      Assert.equal(aAddedData.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
      Assert.equal(aAddedData.index, 4);
      let id = aAddedData.id;

      waitForBookmarkNotification("onItemMoved", function(aMovedData) {
        Assert.equal(aMovedData.id, id);
        Assert.equal(aMovedData.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
        Assert.equal(aMovedData.index, 1);

        run_next_test();
      });

      let txn = PlacesUIUtils.makeTransaction(
        wrapNodeByIdAndParent(aAddedData.id, PlacesUtils.unfiledBookmarksFolderId),
        0, // Unused for real nodes.
        PlacesUtils.unfiledBookmarksFolderId,
        1, // Move to position 1.
        false
      );
      PlacesUtils.transactionManager.doTransaction(txn);
    });

    try {
    let txn = PlacesUIUtils.makeTransaction(
      wrapNodeByIdAndParent(aChangedData.id, PlacesUtils.unfiledBookmarksFolderId),
      0, // Unused for real nodes.
      PlacesUtils.unfiledBookmarksFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      true
    );
    PlacesUtils.transactionManager.doTransaction(txn);
    } catch (ex) {
      do_throw(ex);
    }
  }, "random-anno");

  let id = PlacesUtils.bookmarks.insertSeparator(PlacesUtils.unfiledBookmarksFolderId,
                                                 PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.annotations.setItemAnnotation(id, "random-anno",
                                            "random-value", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
});

add_test(function test_bookmark() {
  const TEST_URL = "http://places.moz.org/";
  const TEST_TITLE = "Places bookmark";

  waitForBookmarkNotification("onItemChanged", function(aChangedData) {
    Assert.equal(aChangedData.title, TEST_TITLE);
    Assert.equal(aChangedData.url, TEST_URL);
    Assert.equal(aChangedData.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    Assert.equal(aChangedData.index, 5);

    waitForBookmarkNotification("onItemAdded", function(aAddedData) {
      Assert.equal(aAddedData.title, TEST_TITLE);
      Assert.equal(aAddedData.url, TEST_URL);
      Assert.equal(aAddedData.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
      Assert.equal(aAddedData.index, 6);
      let id = aAddedData.id;

      waitForBookmarkNotification("onItemMoved", function(aMovedData) {
        Assert.equal(aMovedData.id, id);
        Assert.equal(aMovedData.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
        Assert.equal(aMovedData.index, 1);

        run_next_test();
      });

      let txn = PlacesUIUtils.makeTransaction(
        wrapNodeByIdAndParent(aAddedData.id, PlacesUtils.unfiledBookmarksFolderId),
        0, // Unused for real nodes.
        PlacesUtils.unfiledBookmarksFolderId,
        1, // Move to position 1.
        false
      );
      PlacesUtils.transactionManager.doTransaction(txn);
    });

    try {
    let txn = PlacesUIUtils.makeTransaction(
      wrapNodeByIdAndParent(aChangedData.id, PlacesUtils.unfiledBookmarksFolderId),
      0, // Unused for real nodes.
      PlacesUtils.unfiledBookmarksFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      true
    );
    PlacesUtils.transactionManager.doTransaction(txn);
    } catch (ex) {
      do_throw(ex);
    }
  }, "random-anno");

  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                NetUtil.newURI(TEST_URL),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                TEST_TITLE);
  PlacesUtils.annotations.setItemAnnotation(id, PlacesUIUtils.DESCRIPTION_ANNO,
                                            "description", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setItemAnnotation(id, "random-anno",
                                            "random-value", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
});

add_test(function test_visit() {
  const TEST_URL = "http://places.moz.org/";
  const TEST_TITLE = "Places bookmark";

  waitForBookmarkNotification("onItemAdded", function(aAddedData) {
    Assert.equal(aAddedData.title, TEST_TITLE);
    Assert.equal(aAddedData.url, TEST_URL);
    Assert.equal(aAddedData.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    Assert.equal(aAddedData.index, 7);

    waitForBookmarkNotification("onItemAdded", function(aAddedData2) {
      Assert.equal(aAddedData2.title, TEST_TITLE);
      Assert.equal(aAddedData2.url, TEST_URL);
      Assert.equal(aAddedData2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
      Assert.equal(aAddedData2.index, 8);
      run_next_test();
    });

    try {
    let node = wrapNodeByIdAndParent(aAddedData.id, PlacesUtils.unfiledBookmarksFolderId);
    // Simulate a not-bookmarked node, will copy it to a new bookmark.
    node.id = -1;
    let txn = PlacesUIUtils.makeTransaction(
      node,
      0, // Unused for real nodes.
      PlacesUtils.unfiledBookmarksFolderId,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      true
    );
    PlacesUtils.transactionManager.doTransaction(txn);
    } catch (ex) {
      do_throw(ex);
    }
  });

  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       NetUtil.newURI(TEST_URL),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       TEST_TITLE);
});

add_test(function check_annotations() {
  // As last step check how many items for each annotation exist.

  // Copies should retain the description annotation.
  let descriptions =
    PlacesUtils.annotations.getItemsWithAnnotation(PlacesUIUtils.DESCRIPTION_ANNO, {});
  Assert.equal(descriptions.length, 4);

  // Only the original bookmarks should have this annotation.
  let others = PlacesUtils.annotations.getItemsWithAnnotation("random-anno", {});
  Assert.equal(others.length, 3);
  run_next_test();
});
