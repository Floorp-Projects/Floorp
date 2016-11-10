/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  let unsortedId, ourId;
  let initialBookmarkCount = 0;
  let createdBookmarks = new Set();
  let createdFolderId;
  let collectedEvents = [];
  const nonExistentId = "000000000000";
  const bookmarkGuids = {
    menuGuid:    "menu________",
    toolbarGuid: "toolbar_____",
    unfiledGuid: "unfiled_____",
  };

  function checkOurBookmark(bookmark) {
    browser.test.assertEq(ourId, bookmark.id, "Bookmark has the expected Id");
    browser.test.assertTrue("parentId" in bookmark, "Bookmark has a parentId");
    browser.test.assertEq(0, bookmark.index, "Bookmark has the expected index"); // We assume there are no other bookmarks.
    browser.test.assertEq("http://example.org/", bookmark.url, "Bookmark has the expected url");
    browser.test.assertEq("test bookmark", bookmark.title, "Bookmark has the expected title");
    browser.test.assertTrue("dateAdded" in bookmark, "Bookmark has a dateAdded");
    browser.test.assertFalse("dateGroupModified" in bookmark, "Bookmark does not have a dateGroupModified");
    browser.test.assertFalse("unmodifiable" in bookmark, "Bookmark is not unmodifiable");
  }

  function checkBookmark(expected, bookmark) {
    browser.test.assertEq(expected.url, bookmark.url, "Bookmark has the expected url");
    browser.test.assertEq(expected.title, bookmark.title, "Bookmark has the expected title");
    browser.test.assertEq(expected.index, bookmark.index, "Bookmark has expected index");
    if ("parentId" in expected) {
      browser.test.assertEq(expected.parentId, bookmark.parentId, "Bookmark has the expected parentId");
    }
  }

  function expectedError() {
    browser.test.fail("Did not get expected error");
  }

  function checkOnCreated(id, parentId, index, title, url, dateAdded) {
    let createdData = collectedEvents.pop();
    browser.test.assertEq("onCreated", createdData.event, "onCreated was the last event received");
    browser.test.assertEq(id, createdData.id, "onCreated event received the expected id");
    let bookmark = createdData.bookmark;
    browser.test.assertEq(id, bookmark.id, "onCreated event received the expected bookmark id");
    browser.test.assertEq(parentId, bookmark.parentId, "onCreated event received the expected bookmark parentId");
    browser.test.assertEq(index, bookmark.index, "onCreated event received the expected bookmark index");
    browser.test.assertEq(title, bookmark.title, "onCreated event received the expected bookmark title");
    browser.test.assertEq(url, bookmark.url, "onCreated event received the expected bookmark url");
    browser.test.assertEq(dateAdded, bookmark.dateAdded, "onCreated event received the expected bookmark dateAdded");
  }

  function checkOnChanged(id, url, title) {
    // If both url and title are changed, then url is fired last.
    let changedData = collectedEvents.pop();
    browser.test.assertEq("onChanged", changedData.event, "onChanged was the last event received");
    browser.test.assertEq(id, changedData.id, "onChanged event received the expected id");
    browser.test.assertEq(url, changedData.info.url, "onChanged event received the expected url");
    // title is fired first.
    changedData = collectedEvents.pop();
    browser.test.assertEq("onChanged", changedData.event, "onChanged was the last event received");
    browser.test.assertEq(id, changedData.id, "onChanged event received the expected id");
    browser.test.assertEq(title, changedData.info.title, "onChanged event received the expected title");
  }

  function checkOnMoved(id, parentId, oldParentId, index, oldIndex) {
    let movedData = collectedEvents.pop();
    browser.test.assertEq("onMoved", movedData.event, "onMoved was the last event received");
    browser.test.assertEq(id, movedData.id, "onMoved event received the expected id");
    let info = movedData.info;
    browser.test.assertEq(parentId, info.parentId, "onMoved event received the expected parentId");
    browser.test.assertEq(oldParentId, info.oldParentId, "onMoved event received the expected oldParentId");
    browser.test.assertEq(index, info.index, "onMoved event received the expected index");
    browser.test.assertEq(oldIndex, info.oldIndex, "onMoved event received the expected oldIndex");
  }

  function checkOnRemoved(id, parentId, index, url) {
    let removedData = collectedEvents.pop();
    browser.test.assertEq("onRemoved", removedData.event, "onRemoved was the last event received");
    browser.test.assertEq(id, removedData.id, "onRemoved event received the expected id");
    let info = removedData.info;
    browser.test.assertEq(parentId, removedData.info.parentId, "onRemoved event received the expected parentId");
    browser.test.assertEq(index, removedData.info.index, "onRemoved event received the expected index");
    let node = info.node;
    browser.test.assertEq(id, node.id, "onRemoved event received the expected node id");
    browser.test.assertEq(parentId, node.parentId, "onRemoved event received the expected node parentId");
    browser.test.assertEq(index, node.index, "onRemoved event received the expected node index");
    browser.test.assertEq(url, node.url, "onRemoved event received the expected node url");
  }

  browser.bookmarks.onChanged.addListener((id, info) => {
    collectedEvents.push({event: "onChanged", id, info});
  });

  browser.bookmarks.onCreated.addListener((id, bookmark) => {
    collectedEvents.push({event: "onCreated", id, bookmark});
  });

  browser.bookmarks.onMoved.addListener((id, info) => {
    collectedEvents.push({event: "onMoved", id, info});
  });

  browser.bookmarks.onRemoved.addListener((id, info) => {
    collectedEvents.push({event: "onRemoved", id, info});
  });

  browser.bookmarks.get(["not-a-bookmark-guid"]).then(expectedError, error => {
    browser.test.assertTrue(
      error.message.includes("Invalid value for property 'guid': not-a-bookmark-guid"),
      "Expected error thrown when trying to get a bookmark using an invalid guid"
    );

    return browser.bookmarks.get([nonExistentId]).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Bookmark not found"),
        "Expected error thrown when trying to get a bookmark using a non-existent Id"
      );
    });
  }).then(() => {
    return browser.bookmarks.search({});
  }).then(results => {
    initialBookmarkCount = results.length;
    return browser.bookmarks.create({title: "test bookmark", url: "http://example.org"});
  }).then(result => {
    ourId = result.id;
    checkOurBookmark(result);
    browser.test.assertEq(1, collectedEvents.length, "1 expected event received");
    checkOnCreated(ourId, bookmarkGuids.unfiledGuid, 0, "test bookmark", "http://example.org/", result.dateAdded);

    return browser.bookmarks.get(ourId);
  }).then(results => {
    browser.test.assertEq(results.length, 1);
    checkOurBookmark(results[0]);

    unsortedId = results[0].parentId;
    return browser.bookmarks.get(unsortedId);
  }).then(results => {
    let folder = results[0];
    browser.test.assertEq(1, results.length, "1 bookmark was returned");

    browser.test.assertEq(unsortedId, folder.id, "Folder has the expected id");
    browser.test.assertTrue("parentId" in folder, "Folder has a parentId");
    browser.test.assertTrue("index" in folder, "Folder has an index");
    browser.test.assertFalse("url" in folder, "Folder does not have a url");
    browser.test.assertEq("Other Bookmarks", folder.title, "Folder has the expected title");
    browser.test.assertTrue("dateAdded" in folder, "Folder has a dateAdded");
    browser.test.assertTrue("dateGroupModified" in folder, "Folder has a dateGroupModified");
    browser.test.assertFalse("unmodifiable" in folder, "Folder is not unmodifiable"); // TODO: Do we want to enable this?

    return browser.bookmarks.getChildren(unsortedId);
  }).then(results => {
    browser.test.assertEq(1, results.length, "The folder has one child");
    checkOurBookmark(results[0]);

    return browser.bookmarks.update(nonExistentId, {title: "new test title"}).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("No bookmarks found for the provided GUID"),
        "Expected error thrown when trying to update a non-existent bookmark"
      );

      return browser.bookmarks.update(ourId, {title: "new test title", url: "http://example.com/"});
    });
  }).then(result => {
    browser.test.assertEq("new test title", result.title, "Updated bookmark has the expected title");
    browser.test.assertEq("http://example.com/", result.url, "Updated bookmark has the expected URL");
    browser.test.assertEq(ourId, result.id, "Updated bookmark has the expected id");

    browser.test.assertEq(2, collectedEvents.length, "2 expected events received");
    checkOnChanged(ourId, "http://example.com/", "new test title");

    return Promise.resolve().then(() => {
      return browser.bookmarks.update(ourId, {url: "this is not a valid url"});
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Invalid bookmark:"),
        "Expected error thrown when trying update with an invalid url"
      );
      return browser.bookmarks.getTree();
    });
  }).then(results => {
    browser.test.assertEq(1, results.length, "getTree returns one result");
    let bookmark = results[0].children.find(bookmark => bookmark.id == unsortedId);
    browser.test.assertEq(
        "Other Bookmarks",
        bookmark.title,
        "Folder returned from getTree has the expected title"
    );

    return browser.bookmarks.create({parentId: "invalid"}).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Invalid bookmark"),
        "Expected error thrown when trying to create a bookmark with an invalid parentId"
      );
      browser.test.assertTrue(
          error.message.includes(`"parentGuid":"invalid"`),
          "Expected error thrown when trying to create a bookmark with an invalid parentId"
      );
    });
  }).then(() => {
    return browser.bookmarks.remove(ourId);
  }).then(result => {
    browser.test.assertEq(undefined, result, "Removing a bookmark returns undefined");

    browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
    checkOnRemoved(ourId, bookmarkGuids.unfiledGuid, 0, "http://example.com/");

    return browser.bookmarks.get(ourId).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Bookmark not found"),
        "Expected error thrown when trying to get a removed bookmark"
      );
    });
  }).then(() => {
    return browser.bookmarks.remove(nonExistentId).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("No bookmarks found for the provided GUID"),
        "Expected error thrown when trying removed a non-existent bookmark"
      );
    });
  }).then(() => {
    // test bookmarks.search
    return Promise.all([
      browser.bookmarks.create({title: "MØzillä", url: "http://møzîllä.örg/"}),
      browser.bookmarks.create({title: "Example", url: "http://example.org/"}),
      browser.bookmarks.create({title: "Mozilla Folder"}),
      browser.bookmarks.create({title: "EFF", url: "http://eff.org/"}),
      browser.bookmarks.create({title: "Menu Item", url: "http://menu.org/", parentId: bookmarkGuids.menuGuid}),
      browser.bookmarks.create({title: "Toolbar Item", url: "http://toolbar.org/", parentId: bookmarkGuids.toolbarGuid}),
    ]);
  }).then(results => {
    browser.test.assertEq(6, collectedEvents.length, "6 expected events received");
    checkOnCreated(results[5].id, bookmarkGuids.toolbarGuid, 0, "Toolbar Item", "http://toolbar.org/", results[5].dateAdded);
    checkOnCreated(results[4].id, bookmarkGuids.menuGuid, 0, "Menu Item", "http://menu.org/", results[4].dateAdded);
    checkOnCreated(results[3].id, bookmarkGuids.unfiledGuid, 0, "EFF", "http://eff.org/", results[3].dateAdded);
    checkOnCreated(results[2].id, bookmarkGuids.unfiledGuid, 0, "Mozilla Folder", undefined, results[2].dateAdded);
    checkOnCreated(results[1].id, bookmarkGuids.unfiledGuid, 0, "Example", "http://example.org/", results[1].dateAdded);
    checkOnCreated(results[0].id, bookmarkGuids.unfiledGuid, 0, "MØzillä", "http://møzîllä.örg/", results[0].dateAdded);

    for (let result of results) {
      if (result.title !== "Mozilla Folder") {
        createdBookmarks.add(result.id);
      }
    }
    let folderResult = results[2];
    createdFolderId = folderResult.id;
    return Promise.all([
      browser.bookmarks.create({title: "Mozilla", url: "http://allizom.org/", parentId: createdFolderId}),
      browser.bookmarks.create({title: "Mozilla Corporation", url: "http://allizom.com/", parentId: createdFolderId}),
      browser.bookmarks.create({title: "Firefox", url: "http://allizom.org/firefox/", parentId: createdFolderId}),
    ]).then(results => {
      browser.test.assertEq(3, collectedEvents.length, "3 expected events received");
      checkOnCreated(results[2].id, createdFolderId, 0, "Firefox", "http://allizom.org/firefox/", results[2].dateAdded);
      checkOnCreated(results[1].id, createdFolderId, 0, "Mozilla Corporation", "http://allizom.com/", results[1].dateAdded);
      checkOnCreated(results[0].id, createdFolderId, 0, "Mozilla", "http://allizom.org/", results[0].dateAdded);

      return browser.bookmarks.create({
        title: "About Mozilla",
        url: "http://allizom.org/about/",
        parentId: createdFolderId,
        index: 1,
      });
    }).then(result => {
      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnCreated(result.id, createdFolderId, 1, "About Mozilla", "http://allizom.org/about/", result.dateAdded);

      // returns all items on empty object
      return browser.bookmarks.search({});
    }).then(results => {
      browser.test.assertTrue(results.length >= 9, "At least as many bookmarks as added were returned by search({})");

      return Promise.resolve().then(() => {
        return browser.bookmarks.remove(createdFolderId);
      }).then(expectedError, error => {
        browser.test.assertTrue(
          error.message.includes("Cannot remove a non-empty folder"),
          "Expected error thrown when trying to remove a non-empty folder"
        );
        return browser.bookmarks.getSubTree(createdFolderId);
      });
    });
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of nodes returned by getSubTree");
    browser.test.assertEq("Mozilla Folder", results[0].title, "Folder has the expected title");
    let children = results[0].children;
    browser.test.assertEq(4, children.length, "Expected number of bookmarks returned by getSubTree");
    browser.test.assertEq("Firefox", children[0].title, "Bookmark has the expected title");
    browser.test.assertEq("About Mozilla", children[1].title, "Bookmark has the expected title");
    browser.test.assertEq(1, children[1].index, "Bookmark has the expected index");
    browser.test.assertEq("Mozilla Corporation", children[2].title, "Bookmark has the expected title");
    browser.test.assertEq("Mozilla", children[3].title, "Bookmark has the expected title");

    // throws an error for invalid query objects
    Promise.resolve().then(() => {
      return browser.bookmarks.search();
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Incorrect argument types for bookmarks.search"),
        "Expected error thrown when trying to search with no arguments"
      );
    });

    Promise.resolve().then(() => {
      return browser.bookmarks.search(null);
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Incorrect argument types for bookmarks.search"),
        "Expected error thrown when trying to search with null as an argument"
      );
    });

    Promise.resolve().then(() => {
      return browser.bookmarks.search(function() {});
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Incorrect argument types for bookmarks.search"),
        "Expected error thrown when trying to search with a function as an argument"
      );
    });

    Promise.resolve().then(() => {
      return browser.bookmarks.search({banana: "banana"});
    }).then(expectedError, error => {
      let substr = `an unexpected "banana" property`;
      browser.test.assertTrue(
        error.message.includes(substr),
        `Expected error ${JSON.stringify(error.message)} to contain ${JSON.stringify(substr)}`);
    });

    Promise.resolve().then(() => {
      return browser.bookmarks.search({url: "spider-man vs. batman"});
    }).then(expectedError, error => {
      let substr = 'must match the format "url"';
      browser.test.assertTrue(
        error.message.includes(substr),
        `Expected error ${JSON.stringify(error.message)} to contain ${JSON.stringify(substr)}`);
    });

    // queries the full url
    return browser.bookmarks.search("http://example.org/");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for url search");
    checkBookmark({title: "Example", url: "http://example.org/", index: 2}, results[0]);

    // queries a partial url
    return browser.bookmarks.search("example.org");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for url search");
    checkBookmark({title: "Example", url: "http://example.org/", index: 2}, results[0]);

    // queries the title
    return browser.bookmarks.search("EFF");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for title search");
    checkBookmark({title: "EFF", url: "http://eff.org/", index: 0, parentId: bookmarkGuids.unfiledGuid}, results[0]);

    // finds menu items
    return browser.bookmarks.search("Menu Item");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for menu item search");
    checkBookmark({title: "Menu Item", url: "http://menu.org/", index: 0, parentId: bookmarkGuids.menuGuid}, results[0]);

    // finds toolbar items
    return browser.bookmarks.search("Toolbar Item");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for toolbar item search");
    checkBookmark({title: "Toolbar Item", url: "http://toolbar.org/", index: 0, parentId: bookmarkGuids.toolbarGuid}, results[0]);

    // finds folders
    return browser.bookmarks.search("Mozilla Folder");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of folders returned");
    browser.test.assertEq("Mozilla Folder", results[0].title, "Folder has the expected title");

    // is case-insensitive
    return browser.bookmarks.search("corporation");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returnedfor case-insensitive search");
    browser.test.assertEq("Mozilla Corporation", results[0].title, "Bookmark has the expected title");

    // is case-insensitive for non-ascii
    return browser.bookmarks.search("MøZILLÄ");
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for non-ascii search");
    browser.test.assertEq("MØzillä", results[0].title, "Bookmark has the expected title");

    // returns multiple results
    return browser.bookmarks.search("allizom");
  }).then(results => {
    browser.test.assertEq(4, results.length, "Expected number of multiple results returned");
    browser.test.assertEq("Mozilla", results[0].title, "Bookmark has the expected title");
    browser.test.assertEq("Mozilla Corporation", results[1].title, "Bookmark has the expected title");
    browser.test.assertEq("Firefox", results[2].title, "Bookmark has the expected title");
    browser.test.assertEq("About Mozilla", results[3].title, "Bookmark has the expected title");

    // accepts a url field
    return browser.bookmarks.search({url: "http://allizom.com/"});
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for url field");
    checkBookmark({title: "Mozilla Corporation", url: "http://allizom.com/", index: 2}, results[0]);

    // normalizes urls
    return browser.bookmarks.search({url: "http://allizom.com"});
  }).then(results => {
    browser.test.assertEq(results.length, 1, "Expected number of results returned for normalized url field");
    checkBookmark({title: "Mozilla Corporation", url: "http://allizom.com/", index: 2}, results[0]);

    // normalizes urls even more
    return browser.bookmarks.search({url: "http:allizom.com"});
  }).then(results => {
    browser.test.assertEq(results.length, 1, "Expected number of results returned for normalized url field");
    checkBookmark({title: "Mozilla Corporation", url: "http://allizom.com/", index: 2}, results[0]);

    // accepts a title field
    return browser.bookmarks.search({title: "Mozilla"});
  }).then(results => {
    browser.test.assertEq(results.length, 1, "Expected number of results returned for title field");
    checkBookmark({title: "Mozilla", url: "http://allizom.org/", index: 3}, results[0]);

    // can combine title and query
    return browser.bookmarks.search({title: "Mozilla", query: "allizom"});
  }).then(results => {
    browser.test.assertEq(1, results.length, "Expected number of results returned for title and query fields");
    checkBookmark({title: "Mozilla", url: "http://allizom.org/", index: 3}, results[0]);

    // uses AND conditions
    return browser.bookmarks.search({title: "EFF", query: "allizom"});
  }).then(results => {
    browser.test.assertEq(
      0,
      results.length,
      "Expected number of results returned for non-matching title and query fields"
    );

    // returns an empty array on item not found
    return browser.bookmarks.search("microsoft");
  }).then(results => {
    browser.test.assertEq(0, results.length, "Expected number of results returned for non-matching search");

    return Promise.resolve().then(() => {
      return browser.bookmarks.getRecent("");
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Incorrect argument types for bookmarks.getRecent"),
        "Expected error thrown when calling getRecent with an empty string"
      );
    });
  }).then(() => {
    return Promise.resolve().then(() => {
      return browser.bookmarks.getRecent(1.234);
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("Incorrect argument types for bookmarks.getRecent"),
        "Expected error thrown when calling getRecent with a decimal number"
      );
    });
  }).then(() => {
    return Promise.all([
      browser.bookmarks.search("corporation"),
      browser.bookmarks.getChildren(bookmarkGuids.menuGuid),
    ]);
  }).then(results => {
    let corporationBookmark = results[0][0];
    let childCount = results[1].length;

    browser.test.assertEq(2, corporationBookmark.index, "Bookmark has the expected index");

    return browser.bookmarks.move(corporationBookmark.id, {index: 0}).then(result => {
      browser.test.assertEq(0, result.index, "Bookmark has the expected index");

      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnMoved(corporationBookmark.id, createdFolderId, createdFolderId, 0, 2);

      return browser.bookmarks.move(corporationBookmark.id, {parentId: bookmarkGuids.menuGuid});
    }).then(result => {
      browser.test.assertEq(bookmarkGuids.menuGuid, result.parentId, "Bookmark has the expected parent");
      browser.test.assertEq(childCount, result.index, "Bookmark has the expected index");

      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnMoved(corporationBookmark.id, bookmarkGuids.menuGuid, createdFolderId, 1, 0);

      return browser.bookmarks.move(corporationBookmark.id, {index: 0});
    }).then(result => {
      browser.test.assertEq(bookmarkGuids.menuGuid, result.parentId, "Bookmark has the expected parent");
      browser.test.assertEq(0, result.index, "Bookmark has the expected index");

      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnMoved(corporationBookmark.id, bookmarkGuids.menuGuid, bookmarkGuids.menuGuid, 0, 1);

      return browser.bookmarks.move(corporationBookmark.id, {parentId: bookmarkGuids.toolbarGuid, index: 1});
    }).then(result => {
      browser.test.assertEq(bookmarkGuids.toolbarGuid, result.parentId, "Bookmark has the expected parent");
      browser.test.assertEq(1, result.index, "Bookmark has the expected index");

      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnMoved(corporationBookmark.id, bookmarkGuids.toolbarGuid, bookmarkGuids.menuGuid, 1, 0);

      createdBookmarks.add(corporationBookmark.id);
    });
  }).then(() => {
    return browser.bookmarks.getRecent(4);
  }).then(results => {
    browser.test.assertEq(4, results.length, "Expected number of results returned by getRecent");
    let prevDate = results[0].dateAdded;
    for (let bookmark of results) {
      browser.test.assertTrue(bookmark.dateAdded <= prevDate, "The recent bookmarks are sorted by dateAdded");
      prevDate = bookmark.dateAdded;
    }
    let bookmarksByTitle = results.sort((a, b) => {
      return a.title.localeCompare(b.title);
    });
    browser.test.assertEq("About Mozilla", bookmarksByTitle[0].title, "Bookmark has the expected title");
    browser.test.assertEq("Firefox", bookmarksByTitle[1].title, "Bookmark has the expected title");
    browser.test.assertEq("Mozilla", bookmarksByTitle[2].title, "Bookmark has the expected title");
    browser.test.assertEq("Mozilla Corporation", bookmarksByTitle[3].title, "Bookmark has the expected title");

    return browser.bookmarks.search({});
  }).then(results => {
    let startBookmarkCount = results.length;

    return browser.bookmarks.search({title: "Mozilla Folder"}).then(result => {
      return browser.bookmarks.removeTree(result[0].id);
    }).then(() => {
      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnRemoved(createdFolderId, bookmarkGuids.unfiledGuid, 1);

      return browser.bookmarks.search({}).then(results => {
        browser.test.assertEq(
          startBookmarkCount - 4,
          results.length,
          "Expected number of results returned after removeTree");
      });
    });
  }).then(() => {
    return browser.bookmarks.create({title: "Empty Folder"});
  }).then(result => {
    let emptyFolderId = result.id;

    browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
    checkOnCreated(emptyFolderId, bookmarkGuids.unfiledGuid, 3, "Empty Folder", undefined, result.dateAdded);

    browser.test.assertEq("Empty Folder", result.title, "Folder has the expected title");
    return browser.bookmarks.remove(emptyFolderId).then(() => {
      browser.test.assertEq(1, collectedEvents.length, "1 expected events received");
      checkOnRemoved(emptyFolderId, bookmarkGuids.unfiledGuid, 3);

      return browser.bookmarks.get(emptyFolderId).then(expectedError, error => {
        browser.test.assertTrue(
          error.message.includes("Bookmark not found"),
          "Expected error thrown when trying to get a removed folder"
        );
      });
    });
  }).then(() => {
    return browser.bookmarks.getChildren(nonExistentId).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("root is null"),
        "Expected error thrown when trying to getChildren for a non-existent folder"
      );
    });
  }).then(() => {
    return Promise.resolve().then(() => {
      return browser.bookmarks.move(nonExistentId, {});
    }).then(expectedError, error => {
      browser.test.assertTrue(
        error.message.includes("No bookmarks found for the provided GUID"),
        "Expected error thrown when calling move with a non-existent bookmark"
      );
    });
  }).then(() => {
    // remove all created bookmarks
    let promises = Array.from(createdBookmarks, guid => browser.bookmarks.remove(guid));
    return Promise.all(promises);
  }).then(() => {
    browser.test.assertEq(createdBookmarks.size, collectedEvents.length, "expected number of events received");

    return browser.bookmarks.search({});
  }).then(results => {
    browser.test.assertEq(initialBookmarkCount, results.length, "All created bookmarks have been removed");

    return browser.test.notifyPass("bookmarks");
  }).catch(error => {
    browser.test.fail(`Error: ${String(error)} :: ${error.stack}`);
    browser.test.notifyFail("bookmarks");
  });
}

let extensionData = {
  background: `(${backgroundScript})()`,
  manifest: {
    permissions: ["bookmarks"],
  },
};

add_task(function* test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();
  yield extension.awaitFinish("bookmarks");
  yield extension.unload();
});
