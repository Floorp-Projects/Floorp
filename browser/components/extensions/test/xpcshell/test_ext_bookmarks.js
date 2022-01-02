/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

add_task(async function test_bookmarks() {
  async function background() {
    let unsortedId, ourId;
    let initialBookmarkCount = 0;
    let createdBookmarks = new Set();
    let createdFolderId;
    let createdSeparatorId;
    let collectedEvents = [];
    const nonExistentId = "000000000000";
    const bookmarkGuids = {
      menuGuid: "menu________",
      toolbarGuid: "toolbar_____",
      unfiledGuid: "unfiled_____",
      rootGuid: "root________",
    };

    function checkOurBookmark(bookmark) {
      browser.test.assertEq(ourId, bookmark.id, "Bookmark has the expected Id");
      browser.test.assertTrue(
        "parentId" in bookmark,
        "Bookmark has a parentId"
      );
      browser.test.assertEq(
        0,
        bookmark.index,
        "Bookmark has the expected index"
      ); // We assume there are no other bookmarks.
      browser.test.assertEq(
        "http://example.org/",
        bookmark.url,
        "Bookmark has the expected url"
      );
      browser.test.assertEq(
        "test bookmark",
        bookmark.title,
        "Bookmark has the expected title"
      );
      browser.test.assertTrue(
        "dateAdded" in bookmark,
        "Bookmark has a dateAdded"
      );
      browser.test.assertFalse(
        "dateGroupModified" in bookmark,
        "Bookmark does not have a dateGroupModified"
      );
      browser.test.assertFalse(
        "unmodifiable" in bookmark,
        "Bookmark is not unmodifiable"
      );
      browser.test.assertEq(
        "bookmark",
        bookmark.type,
        "Bookmark is of type bookmark"
      );
    }

    function checkBookmark(expected, bookmark) {
      browser.test.assertEq(
        expected.url,
        bookmark.url,
        "Bookmark has the expected url"
      );
      browser.test.assertEq(
        expected.title,
        bookmark.title,
        "Bookmark has the expected title"
      );
      browser.test.assertEq(
        expected.index,
        bookmark.index,
        "Bookmark has expected index"
      );
      browser.test.assertEq(
        "bookmark",
        bookmark.type,
        "Bookmark is of type bookmark"
      );
      if ("parentId" in expected) {
        browser.test.assertEq(
          expected.parentId,
          bookmark.parentId,
          "Bookmark has the expected parentId"
        );
      }
    }

    function checkOnCreated(
      id,
      parentId,
      index,
      title,
      url,
      dateAdded,
      type = "bookmark"
    ) {
      let createdData = collectedEvents.pop();
      browser.test.assertEq(
        "onCreated",
        createdData.event,
        "onCreated was the last event received"
      );
      browser.test.assertEq(
        id,
        createdData.id,
        "onCreated event received the expected id"
      );
      let bookmark = createdData.bookmark;
      browser.test.assertEq(
        id,
        bookmark.id,
        "onCreated event received the expected bookmark id"
      );
      browser.test.assertEq(
        parentId,
        bookmark.parentId,
        "onCreated event received the expected bookmark parentId"
      );
      browser.test.assertEq(
        index,
        bookmark.index,
        "onCreated event received the expected bookmark index"
      );
      browser.test.assertEq(
        title,
        bookmark.title,
        "onCreated event received the expected bookmark title"
      );
      browser.test.assertEq(
        url,
        bookmark.url,
        "onCreated event received the expected bookmark url"
      );
      browser.test.assertEq(
        dateAdded,
        bookmark.dateAdded,
        "onCreated event received the expected bookmark dateAdded"
      );
      browser.test.assertEq(
        type,
        bookmark.type,
        "onCreated event received the expected bookmark type"
      );
    }

    function checkOnChanged(id, url, title) {
      // If both url and title are changed, then url is fired last.
      let changedData = collectedEvents.pop();
      browser.test.assertEq(
        "onChanged",
        changedData.event,
        "onChanged was the last event received"
      );
      browser.test.assertEq(
        id,
        changedData.id,
        "onChanged event received the expected id"
      );
      browser.test.assertEq(
        url,
        changedData.info.url,
        "onChanged event received the expected url"
      );
      // title is fired first.
      changedData = collectedEvents.pop();
      browser.test.assertEq(
        "onChanged",
        changedData.event,
        "onChanged was the last event received"
      );
      browser.test.assertEq(
        id,
        changedData.id,
        "onChanged event received the expected id"
      );
      browser.test.assertEq(
        title,
        changedData.info.title,
        "onChanged event received the expected title"
      );
    }

    function checkOnMoved(id, parentId, oldParentId, index, oldIndex) {
      let movedData = collectedEvents.pop();
      browser.test.assertEq(
        "onMoved",
        movedData.event,
        "onMoved was the last event received"
      );
      browser.test.assertEq(
        id,
        movedData.id,
        "onMoved event received the expected id"
      );
      let info = movedData.info;
      browser.test.assertEq(
        parentId,
        info.parentId,
        "onMoved event received the expected parentId"
      );
      browser.test.assertEq(
        oldParentId,
        info.oldParentId,
        "onMoved event received the expected oldParentId"
      );
      browser.test.assertEq(
        index,
        info.index,
        "onMoved event received the expected index"
      );
      browser.test.assertEq(
        oldIndex,
        info.oldIndex,
        "onMoved event received the expected oldIndex"
      );
    }

    function checkOnRemoved(id, parentId, index, url, type = "folder") {
      let removedData = collectedEvents.pop();
      browser.test.assertEq(
        "onRemoved",
        removedData.event,
        "onRemoved was the last event received"
      );
      browser.test.assertEq(
        id,
        removedData.id,
        "onRemoved event received the expected id"
      );
      let info = removedData.info;
      browser.test.assertEq(
        parentId,
        removedData.info.parentId,
        "onRemoved event received the expected parentId"
      );
      browser.test.assertEq(
        index,
        removedData.info.index,
        "onRemoved event received the expected index"
      );
      let node = info.node;
      browser.test.assertEq(
        id,
        node.id,
        "onRemoved event received the expected node id"
      );
      browser.test.assertEq(
        parentId,
        node.parentId,
        "onRemoved event received the expected node parentId"
      );
      browser.test.assertEq(
        index,
        node.index,
        "onRemoved event received the expected node index"
      );
      browser.test.assertEq(
        url,
        node.url,
        "onRemoved event received the expected node url"
      );
      browser.test.assertEq(
        type,
        node.type,
        "onRemoved event received the expected node type"
      );
    }

    browser.bookmarks.onChanged.addListener((id, info) => {
      collectedEvents.push({ event: "onChanged", id, info });
    });

    browser.bookmarks.onCreated.addListener((id, bookmark) => {
      collectedEvents.push({ event: "onCreated", id, bookmark });
    });

    browser.bookmarks.onMoved.addListener((id, info) => {
      collectedEvents.push({ event: "onMoved", id, info });
    });

    browser.bookmarks.onRemoved.addListener((id, info) => {
      collectedEvents.push({ event: "onRemoved", id, info });
    });

    await browser.test.assertRejects(
      browser.bookmarks.get(["not-a-bookmark-guid"]),
      /Invalid value for property 'guid': "not-a-bookmark-guid"/,
      "Expected error thrown when trying to get a bookmark using an invalid guid"
    );

    await browser.test
      .assertRejects(
        browser.bookmarks.get([nonExistentId]),
        /Bookmark not found/,
        "Expected error thrown when trying to get a bookmark using a non-existent Id"
      )
      .then(() => {
        return browser.bookmarks.search({});
      })
      .then(results => {
        initialBookmarkCount = results.length;
        return browser.bookmarks.create({
          title: "test bookmark",
          url: "http://example.org",
          type: "bookmark",
        });
      })
      .then(result => {
        ourId = result.id;
        checkOurBookmark(result);
        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected event received"
        );
        checkOnCreated(
          ourId,
          bookmarkGuids.unfiledGuid,
          0,
          "test bookmark",
          "http://example.org/",
          result.dateAdded
        );

        return browser.bookmarks.get(ourId);
      })
      .then(results => {
        browser.test.assertEq(results.length, 1);
        checkOurBookmark(results[0]);

        unsortedId = results[0].parentId;
        return browser.bookmarks.get(unsortedId);
      })
      .then(results => {
        let folder = results[0];
        browser.test.assertEq(1, results.length, "1 bookmark was returned");

        browser.test.assertEq(
          unsortedId,
          folder.id,
          "Folder has the expected id"
        );
        browser.test.assertTrue("parentId" in folder, "Folder has a parentId");
        browser.test.assertTrue("index" in folder, "Folder has an index");
        browser.test.assertEq(
          undefined,
          folder.url,
          "Folder does not have a url"
        );
        browser.test.assertEq(
          "Other Bookmarks",
          folder.title,
          "Folder has the expected title"
        );
        browser.test.assertTrue(
          "dateAdded" in folder,
          "Folder has a dateAdded"
        );
        browser.test.assertTrue(
          "dateGroupModified" in folder,
          "Folder has a dateGroupModified"
        );
        browser.test.assertFalse(
          "unmodifiable" in folder,
          "Folder is not unmodifiable"
        ); // TODO: Do we want to enable this?
        browser.test.assertEq(
          "folder",
          folder.type,
          "Folder has a type of folder"
        );

        return browser.bookmarks.getChildren(unsortedId);
      })
      .then(async results => {
        browser.test.assertEq(1, results.length, "The folder has one child");
        checkOurBookmark(results[0]);

        await browser.test.assertRejects(
          browser.bookmarks.update(nonExistentId, { title: "new test title" }),
          /No bookmarks found for the provided GUID/,
          "Expected error thrown when trying to update a non-existent bookmark"
        );
        return browser.bookmarks.update(ourId, {
          title: "new test title",
          url: "http://example.com/",
        });
      })
      .then(async result => {
        browser.test.assertEq(
          "new test title",
          result.title,
          "Updated bookmark has the expected title"
        );
        browser.test.assertEq(
          "http://example.com/",
          result.url,
          "Updated bookmark has the expected URL"
        );
        browser.test.assertEq(
          ourId,
          result.id,
          "Updated bookmark has the expected id"
        );
        browser.test.assertEq(
          "bookmark",
          result.type,
          "Updated bookmark has a type of bookmark"
        );

        browser.test.assertEq(
          2,
          collectedEvents.length,
          "2 expected events received"
        );
        checkOnChanged(ourId, "http://example.com/", "new test title");

        await browser.test.assertRejects(
          browser.bookmarks.update(ourId, { url: "this is not a valid url" }),
          /Invalid bookmark:/,
          "Expected error thrown when trying update with an invalid url"
        );
        return browser.bookmarks.getTree();
      })
      .then(results => {
        browser.test.assertEq(1, results.length, "getTree returns one result");
        let bookmark = results[0].children.find(
          bookmarkItem => bookmarkItem.id == unsortedId
        );
        browser.test.assertEq(
          "Other Bookmarks",
          bookmark.title,
          "Folder returned from getTree has the expected title"
        );
        browser.test.assertEq(
          "folder",
          bookmark.type,
          "Folder returned from getTree has the expected type"
        );

        return browser.test.assertRejects(
          browser.bookmarks.create({ parentId: "invalid" }),
          error =>
            error.message.includes("Invalid bookmark") &&
            error.message.includes(`"parentGuid":"invalid"`),
          "Expected error thrown when trying to create a bookmark with an invalid parentId"
        );
      })
      .then(() => {
        return browser.bookmarks.remove(ourId);
      })
      .then(result => {
        browser.test.assertEq(
          undefined,
          result,
          "Removing a bookmark returns undefined"
        );

        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnRemoved(
          ourId,
          bookmarkGuids.unfiledGuid,
          0,
          "http://example.com/",
          "bookmark"
        );

        return browser.test.assertRejects(
          browser.bookmarks.get(ourId),
          /Bookmark not found/,
          "Expected error thrown when trying to get a removed bookmark"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.remove(nonExistentId),
          /No bookmarks found for the provided GUID/,
          "Expected error thrown when trying removed a non-existent bookmark"
        );
      })
      .then(() => {
        // test bookmarks.search
        return Promise.all([
          browser.bookmarks.create({
            title: "Μοζιλλας",
            url: "http://møzîllä.örg/",
          }),
          browser.bookmarks.create({
            title: "Example",
            url: "http://example.org/",
          }),
          browser.bookmarks.create({ title: "Mozilla Folder", type: "folder" }),
          browser.bookmarks.create({ title: "EFF", url: "http://eff.org/" }),
          browser.bookmarks.create({
            title: "Menu Item",
            url: "http://menu.org/",
            parentId: bookmarkGuids.menuGuid,
          }),
          browser.bookmarks.create({
            title: "Toolbar Item",
            url: "http://toolbar.org/",
            parentId: bookmarkGuids.toolbarGuid,
          }),
        ]);
      })
      .then(results => {
        browser.test.assertEq(
          6,
          collectedEvents.length,
          "6 expected events received"
        );
        checkOnCreated(
          results[5].id,
          bookmarkGuids.toolbarGuid,
          0,
          "Toolbar Item",
          "http://toolbar.org/",
          results[5].dateAdded
        );
        checkOnCreated(
          results[4].id,
          bookmarkGuids.menuGuid,
          0,
          "Menu Item",
          "http://menu.org/",
          results[4].dateAdded
        );
        checkOnCreated(
          results[3].id,
          bookmarkGuids.unfiledGuid,
          0,
          "EFF",
          "http://eff.org/",
          results[3].dateAdded
        );
        checkOnCreated(
          results[2].id,
          bookmarkGuids.unfiledGuid,
          0,
          "Mozilla Folder",
          undefined,
          results[2].dateAdded,
          "folder"
        );
        checkOnCreated(
          results[1].id,
          bookmarkGuids.unfiledGuid,
          0,
          "Example",
          "http://example.org/",
          results[1].dateAdded
        );
        checkOnCreated(
          results[0].id,
          bookmarkGuids.unfiledGuid,
          0,
          "Μοζιλλας",
          "http://xn--mzll-ooa1dud.xn--rg-eka/",
          results[0].dateAdded
        );

        for (let result of results) {
          if (result.title !== "Mozilla Folder") {
            createdBookmarks.add(result.id);
          }
        }
        let folderResult = results[2];
        createdFolderId = folderResult.id;
        return Promise.all([
          browser.bookmarks.create({
            title: "Mozilla",
            url: "http://allizom.org/",
            parentId: createdFolderId,
          }),
          browser.bookmarks.create({
            parentId: createdFolderId,
            type: "separator",
          }),
          browser.bookmarks.create({
            title: "Mozilla Corporation",
            url: "http://allizom.com/",
            parentId: createdFolderId,
          }),
          browser.bookmarks.create({
            title: "Firefox",
            url: "http://allizom.org/firefox/",
            parentId: createdFolderId,
          }),
        ])
          .then(newBookmarks => {
            browser.test.assertEq(
              4,
              collectedEvents.length,
              "4 expected events received"
            );
            checkOnCreated(
              newBookmarks[3].id,
              createdFolderId,
              0,
              "Firefox",
              "http://allizom.org/firefox/",
              newBookmarks[3].dateAdded
            );
            checkOnCreated(
              newBookmarks[2].id,
              createdFolderId,
              0,
              "Mozilla Corporation",
              "http://allizom.com/",
              newBookmarks[2].dateAdded
            );
            checkOnCreated(
              newBookmarks[1].id,
              createdFolderId,
              0,
              "",
              "data:",
              newBookmarks[1].dateAdded,
              "separator"
            );
            checkOnCreated(
              newBookmarks[0].id,
              createdFolderId,
              0,
              "Mozilla",
              "http://allizom.org/",
              newBookmarks[0].dateAdded
            );

            return browser.bookmarks.create({
              title: "About Mozilla",
              url: "http://allizom.org/about/",
              parentId: createdFolderId,
              index: 1,
            });
          })
          .then(result => {
            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnCreated(
              result.id,
              createdFolderId,
              1,
              "About Mozilla",
              "http://allizom.org/about/",
              result.dateAdded
            );

            // returns all items on empty object
            return browser.bookmarks.search({});
          })
          .then(async bookmarksSearchResults => {
            browser.test.assertTrue(
              bookmarksSearchResults.length >= 10,
              "At least as many bookmarks as added were returned by search({})"
            );

            await browser.test.assertRejects(
              browser.bookmarks.remove(createdFolderId),
              /Cannot remove a non-empty folder/,
              "Expected error thrown when trying to remove a non-empty folder"
            );
            return browser.bookmarks.getSubTree(createdFolderId);
          });
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of nodes returned by getSubTree"
        );
        browser.test.assertEq(
          "Mozilla Folder",
          results[0].title,
          "Folder has the expected title"
        );
        browser.test.assertEq(
          bookmarkGuids.unfiledGuid,
          results[0].parentId,
          "Folder has the expected parentId"
        );
        browser.test.assertEq(
          "folder",
          results[0].type,
          "Folder has the expected type"
        );
        let children = results[0].children;
        browser.test.assertEq(
          5,
          children.length,
          "Expected number of bookmarks returned by getSubTree"
        );
        browser.test.assertEq(
          "Firefox",
          children[0].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "bookmark",
          children[0].type,
          "Bookmark has the expected type"
        );
        browser.test.assertEq(
          "About Mozilla",
          children[1].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "bookmark",
          children[1].type,
          "Bookmark has the expected type"
        );
        browser.test.assertEq(
          1,
          children[1].index,
          "Bookmark has the expected index"
        );
        browser.test.assertEq(
          "Mozilla Corporation",
          children[2].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "",
          children[3].title,
          "Separator has the expected title"
        );
        browser.test.assertEq(
          "data:",
          children[3].url,
          "Separator has the expected url"
        );
        browser.test.assertEq(
          "separator",
          children[3].type,
          "Separator has the expected type"
        );
        browser.test.assertEq(
          "Mozilla",
          children[4].title,
          "Bookmark has the expected title"
        );

        // throws an error for invalid query objects
        browser.test.assertThrows(
          () => browser.bookmarks.search(),
          /Incorrect argument types for bookmarks.search/,
          "Expected error thrown when trying to search with no arguments"
        );

        browser.test.assertThrows(
          () => browser.bookmarks.search(null),
          /Incorrect argument types for bookmarks.search/,
          "Expected error thrown when trying to search with null as an argument"
        );

        browser.test.assertThrows(
          () => browser.bookmarks.search(() => {}),
          /Incorrect argument types for bookmarks.search/,
          "Expected error thrown when trying to search with a function as an argument"
        );

        browser.test.assertThrows(
          () => browser.bookmarks.search({ banana: "banana" }),
          /an unexpected "banana" property/,
          "Expected error thrown when trying to search with a banana as an argument"
        );

        browser.test.assertThrows(
          () => browser.bookmarks.search({ url: "spider-man vs. batman" }),
          /must match the format "url"/,
          "Expected error thrown when trying to search with a illegally formatted URL"
        );
        // queries the full url
        return browser.bookmarks.search("http://example.org/");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for url search"
        );
        checkBookmark(
          { title: "Example", url: "http://example.org/", index: 2 },
          results[0]
        );

        // queries a partial url
        return browser.bookmarks.search("example.org");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for url search"
        );
        checkBookmark(
          { title: "Example", url: "http://example.org/", index: 2 },
          results[0]
        );

        // queries the title
        return browser.bookmarks.search("EFF");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for title search"
        );
        checkBookmark(
          {
            title: "EFF",
            url: "http://eff.org/",
            index: 0,
            parentId: bookmarkGuids.unfiledGuid,
          },
          results[0]
        );

        // finds menu items
        return browser.bookmarks.search("Menu Item");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for menu item search"
        );
        checkBookmark(
          {
            title: "Menu Item",
            url: "http://menu.org/",
            index: 0,
            parentId: bookmarkGuids.menuGuid,
          },
          results[0]
        );

        // finds toolbar items
        return browser.bookmarks.search("Toolbar Item");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for toolbar item search"
        );
        checkBookmark(
          {
            title: "Toolbar Item",
            url: "http://toolbar.org/",
            index: 0,
            parentId: bookmarkGuids.toolbarGuid,
          },
          results[0]
        );

        // finds folders
        return browser.bookmarks.search("Mozilla Folder");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of folders returned"
        );
        browser.test.assertEq(
          "Mozilla Folder",
          results[0].title,
          "Folder has the expected title"
        );
        browser.test.assertEq(
          "folder",
          results[0].type,
          "Folder has the expected type"
        );

        // is case-insensitive
        return browser.bookmarks.search("corporation");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returnedfor case-insensitive search"
        );
        browser.test.assertEq(
          "Mozilla Corporation",
          results[0].title,
          "Bookmark has the expected title"
        );

        // is case-insensitive for non-ascii
        return browser.bookmarks.search("ΜοΖΙΛΛΑς");
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for non-ascii search"
        );
        browser.test.assertEq(
          "Μοζιλλας",
          results[0].title,
          "Bookmark has the expected title"
        );

        // returns multiple results
        return browser.bookmarks.search("allizom");
      })
      .then(results => {
        browser.test.assertEq(
          4,
          results.length,
          "Expected number of multiple results returned"
        );
        browser.test.assertEq(
          "Mozilla",
          results[0].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "Mozilla Corporation",
          results[1].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "Firefox",
          results[2].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "About Mozilla",
          results[3].title,
          "Bookmark has the expected title"
        );

        // accepts a url field
        return browser.bookmarks.search({ url: "http://allizom.com/" });
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for url field"
        );
        checkBookmark(
          {
            title: "Mozilla Corporation",
            url: "http://allizom.com/",
            index: 2,
          },
          results[0]
        );

        // normalizes urls
        return browser.bookmarks.search({ url: "http://allizom.com" });
      })
      .then(results => {
        browser.test.assertEq(
          results.length,
          1,
          "Expected number of results returned for normalized url field"
        );
        checkBookmark(
          {
            title: "Mozilla Corporation",
            url: "http://allizom.com/",
            index: 2,
          },
          results[0]
        );

        // normalizes urls even more
        return browser.bookmarks.search({ url: "http:allizom.com" });
      })
      .then(results => {
        browser.test.assertEq(
          results.length,
          1,
          "Expected number of results returned for normalized url field"
        );
        checkBookmark(
          {
            title: "Mozilla Corporation",
            url: "http://allizom.com/",
            index: 2,
          },
          results[0]
        );

        // accepts a title field
        return browser.bookmarks.search({ title: "Mozilla" });
      })
      .then(results => {
        browser.test.assertEq(
          results.length,
          1,
          "Expected number of results returned for title field"
        );
        checkBookmark(
          { title: "Mozilla", url: "http://allizom.org/", index: 4 },
          results[0]
        );

        // can combine title and query
        return browser.bookmarks.search({ title: "Mozilla", query: "allizom" });
      })
      .then(results => {
        browser.test.assertEq(
          1,
          results.length,
          "Expected number of results returned for title and query fields"
        );
        checkBookmark(
          { title: "Mozilla", url: "http://allizom.org/", index: 4 },
          results[0]
        );

        // uses AND conditions
        return browser.bookmarks.search({ title: "EFF", query: "allizom" });
      })
      .then(results => {
        browser.test.assertEq(
          0,
          results.length,
          "Expected number of results returned for non-matching title and query fields"
        );

        // returns an empty array on item not found
        return browser.bookmarks.search("microsoft");
      })
      .then(results => {
        browser.test.assertEq(
          0,
          results.length,
          "Expected number of results returned for non-matching search"
        );

        browser.test.assertThrows(
          () => browser.bookmarks.getRecent(""),
          /Incorrect argument types for bookmarks.getRecent/,
          "Expected error thrown when calling getRecent with an empty string"
        );
      })
      .then(() => {
        browser.test.assertThrows(
          () => browser.bookmarks.getRecent(1.234),
          /Incorrect argument types for bookmarks.getRecent/,
          "Expected error thrown when calling getRecent with a decimal number"
        );
      })
      .then(() => {
        return Promise.all([
          browser.bookmarks.search("corporation"),
          browser.bookmarks.getChildren(bookmarkGuids.menuGuid),
        ]);
      })
      .then(results => {
        let corporationBookmark = results[0][0];
        let childCount = results[1].length;

        browser.test.assertEq(
          2,
          corporationBookmark.index,
          "Bookmark has the expected index"
        );

        return browser.bookmarks
          .move(corporationBookmark.id, { index: 0 })
          .then(result => {
            browser.test.assertEq(
              0,
              result.index,
              "Bookmark has the expected index"
            );

            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnMoved(
              corporationBookmark.id,
              createdFolderId,
              createdFolderId,
              0,
              2
            );

            return browser.bookmarks.move(corporationBookmark.id, {
              parentId: bookmarkGuids.menuGuid,
            });
          })
          .then(result => {
            browser.test.assertEq(
              bookmarkGuids.menuGuid,
              result.parentId,
              "Bookmark has the expected parent"
            );
            browser.test.assertEq(
              childCount,
              result.index,
              "Bookmark has the expected index"
            );

            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnMoved(
              corporationBookmark.id,
              bookmarkGuids.menuGuid,
              createdFolderId,
              1,
              0
            );

            return browser.bookmarks.move(corporationBookmark.id, { index: 0 });
          })
          .then(result => {
            browser.test.assertEq(
              bookmarkGuids.menuGuid,
              result.parentId,
              "Bookmark has the expected parent"
            );
            browser.test.assertEq(
              0,
              result.index,
              "Bookmark has the expected index"
            );

            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnMoved(
              corporationBookmark.id,
              bookmarkGuids.menuGuid,
              bookmarkGuids.menuGuid,
              0,
              1
            );

            return browser.bookmarks.move(corporationBookmark.id, {
              parentId: bookmarkGuids.toolbarGuid,
              index: 1,
            });
          })
          .then(result => {
            browser.test.assertEq(
              bookmarkGuids.toolbarGuid,
              result.parentId,
              "Bookmark has the expected parent"
            );
            browser.test.assertEq(
              1,
              result.index,
              "Bookmark has the expected index"
            );

            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnMoved(
              corporationBookmark.id,
              bookmarkGuids.toolbarGuid,
              bookmarkGuids.menuGuid,
              1,
              0
            );

            createdBookmarks.add(corporationBookmark.id);
          });
      })
      .then(() => {
        return browser.bookmarks.getRecent(4);
      })
      .then(results => {
        browser.test.assertEq(
          4,
          results.length,
          "Expected number of results returned by getRecent"
        );
        let prevDate = results[0].dateAdded;
        for (let bookmark of results) {
          browser.test.assertTrue(
            bookmark.dateAdded <= prevDate,
            "The recent bookmarks are sorted by dateAdded"
          );
          prevDate = bookmark.dateAdded;
        }
        let bookmarksByTitle = results.sort((a, b) => {
          return a.title.localeCompare(b.title);
        });
        browser.test.assertEq(
          "About Mozilla",
          bookmarksByTitle[0].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "Firefox",
          bookmarksByTitle[1].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "Mozilla",
          bookmarksByTitle[2].title,
          "Bookmark has the expected title"
        );
        browser.test.assertEq(
          "Mozilla Corporation",
          bookmarksByTitle[3].title,
          "Bookmark has the expected title"
        );

        return browser.bookmarks.search({});
      })
      .then(results => {
        let startBookmarkCount = results.length;

        return browser.bookmarks
          .search({ title: "Mozilla Folder" })
          .then(result => {
            return browser.bookmarks.removeTree(result[0].id);
          })
          .then(() => {
            browser.test.assertEq(
              1,
              collectedEvents.length,
              "1 expected events received"
            );
            checkOnRemoved(createdFolderId, bookmarkGuids.unfiledGuid, 1);

            return browser.bookmarks.search({}).then(searchResults => {
              browser.test.assertEq(
                startBookmarkCount - 5,
                searchResults.length,
                "Expected number of results returned after removeTree"
              );
            });
          });
      })
      .then(() => {
        return browser.bookmarks.create({ title: "Empty Folder" });
      })
      .then(result => {
        createdFolderId = result.id;

        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnCreated(
          createdFolderId,
          bookmarkGuids.unfiledGuid,
          3,
          "Empty Folder",
          undefined,
          result.dateAdded,
          "folder"
        );

        browser.test.assertEq(
          "Empty Folder",
          result.title,
          "Folder has the expected title"
        );
        browser.test.assertEq(
          "folder",
          result.type,
          "Folder has the expected type"
        );

        return browser.bookmarks.create({
          parentId: createdFolderId,
          type: "separator",
        });
      })
      .then(result => {
        createdSeparatorId = result.id;
        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnCreated(
          createdSeparatorId,
          createdFolderId,
          0,
          "",
          "data:",
          result.dateAdded,
          "separator"
        );
        return browser.bookmarks.remove(createdSeparatorId);
      })
      .then(() => {
        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnRemoved(
          createdSeparatorId,
          createdFolderId,
          0,
          "data:",
          "separator"
        );

        return browser.bookmarks.remove(createdFolderId);
      })
      .then(() => {
        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnRemoved(createdFolderId, bookmarkGuids.unfiledGuid, 3);

        return browser.test.assertRejects(
          browser.bookmarks.get(createdFolderId),
          /Bookmark not found/,
          "Expected error thrown when trying to get a removed folder"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.getChildren(nonExistentId),
          /root is null/,
          "Expected error thrown when trying to getChildren for a non-existent folder"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.move(nonExistentId, {}),
          /No bookmarks found for the provided GUID/,
          "Expected error thrown when calling move with a non-existent bookmark"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.create({
            title: "test root folder",
            parentId: bookmarkGuids.rootGuid,
          }),
          "The bookmark root cannot be modified",
          "Expected error thrown when creating bookmark folder at the root"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.update(bookmarkGuids.rootGuid, {
            title: "test update title",
          }),
          "The bookmark root cannot be modified",
          "Expected error thrown when updating root"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.remove(bookmarkGuids.rootGuid),
          "The bookmark root cannot be modified",
          "Expected error thrown when removing root"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.removeTree(bookmarkGuids.rootGuid),
          "The bookmark root cannot be modified",
          "Expected error thrown when removing root tree"
        );
      })
      .then(() => {
        return browser.bookmarks.create({ title: "Empty Folder" });
      })
      .then(async result => {
        createdFolderId = result.id;

        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnCreated(
          createdFolderId,
          bookmarkGuids.unfiledGuid,
          3,
          "Empty Folder",
          undefined,
          result.dateAdded,
          "folder"
        );

        await browser.test.assertRejects(
          browser.bookmarks.move(createdFolderId, {
            parentId: bookmarkGuids.rootGuid,
          }),
          "The bookmark root cannot be modified",
          "Expected error thrown when moving bookmark folder to the root"
        );

        return browser.bookmarks.remove(createdFolderId);
      })
      .then(() => {
        browser.test.assertEq(
          1,
          collectedEvents.length,
          "1 expected events received"
        );
        checkOnRemoved(
          createdFolderId,
          bookmarkGuids.unfiledGuid,
          3,
          undefined,
          "folder"
        );

        return browser.test.assertRejects(
          browser.bookmarks.get(createdFolderId),
          "Bookmark not found",
          "Expected error thrown when trying to get a removed folder"
        );
      })
      .then(() => {
        return browser.test.assertRejects(
          browser.bookmarks.move(bookmarkGuids.rootGuid, {
            parentId: bookmarkGuids.unfiledGuid,
          }),
          "The bookmark root cannot be modified",
          "Expected error thrown when moving root"
        );
      })
      .then(() => {
        // remove all created bookmarks
        let promises = Array.from(createdBookmarks, guid =>
          browser.bookmarks.remove(guid)
        );
        return Promise.all(promises);
      })
      .then(() => {
        browser.test.assertEq(
          createdBookmarks.size,
          collectedEvents.length,
          "expected number of events received"
        );

        return browser.bookmarks.search({});
      })
      .then(results => {
        browser.test.assertEq(
          initialBookmarkCount,
          results.length,
          "All created bookmarks have been removed"
        );

        return browser.test.notifyPass("bookmarks");
      })
      .catch(error => {
        browser.test.fail(`Error: ${String(error)} :: ${error.stack}`);
        browser.test.notifyFail("bookmarks");
      });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["bookmarks"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("bookmarks");
  await extension.unload();
});

add_task(async function test_get_recent_with_tag_and_query() {
  function background() {
    browser.bookmarks.getRecent(100).then(bookmarks => {
      browser.test.sendMessage("bookmarks", bookmarks);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["bookmarks"],
    },
  });

  // Start with an empty bookmarks database.
  await PlacesUtils.bookmarks.eraseEverything();

  let createdBookmarks = [];
  for (let i = 0; i < 3; i++) {
    let bookmark = {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: `http://example.com/${i}`,
      title: `My bookmark ${i}`,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    };
    createdBookmarks.unshift(bookmark);
    await PlacesUtils.bookmarks.insert(bookmark);
  }

  // Add a tag to the most recent url to prove it doesn't get returned.
  PlacesUtils.tagging.tagURI(NetUtil.newURI("http://example.com/${i}"), [
    "Test Tag",
  ]);

  // Add a query bookmark.
  let queryURL = `place:parent=${PlacesUtils.bookmarks.menuGuid}&queryType=1`;
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: queryURL,
    title: "a test query",
  });

  await extension.startup();
  let receivedBookmarks = await extension.awaitMessage("bookmarks");

  equal(
    receivedBookmarks.length,
    3,
    "The expected number of bookmarks was returned."
  );
  for (let i = 0; i < 3; i++) {
    let actual = receivedBookmarks[i];
    let expected = createdBookmarks[i];
    equal(actual.url, expected.url, "Bookmark has the expected url.");
    equal(actual.title, expected.title, "Bookmark has the expected title.");
    equal(
      actual.parentId,
      expected.parentGuid,
      "Bookmark has the expected parentId."
    );
  }

  await extension.unload();
});

add_task(async function test_tree_with_empty_folder() {
  async function background() {
    await browser.bookmarks.create({ title: "Empty Folder" });
    let nonEmptyFolder = await browser.bookmarks.create({
      title: "Non-Empty Folder",
    });
    await browser.bookmarks.create({
      title: "A bookmark",
      url: "http://example.com",
      parentId: nonEmptyFolder.id,
    });

    let tree = await browser.bookmarks.getSubTree(nonEmptyFolder.parentId);
    browser.test.assertEq(
      0,
      tree[0].children[0].children.length,
      "The empty folder returns an empty array for children."
    );
    browser.test.assertEq(
      1,
      tree[0].children[1].children.length,
      "The non-empty folder returns a single item array for children."
    );

    let children = await browser.bookmarks.getChildren(nonEmptyFolder.parentId);
    // getChildren should only return immediate children. This is not tested in the
    // monster test above.
    for (let child of children) {
      browser.test.assertEq(
        undefined,
        child.children,
        "Child from getChildren does not contain any children."
      );
    }

    browser.test.sendMessage("done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["bookmarks"],
    },
  });

  // Start with an empty bookmarks database.
  await PlacesUtils.bookmarks.eraseEverything();

  await extension.startup();
  await extension.awaitMessage("done");

  await extension.unload();
});
