/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* globals ChromeUtils, Assert, add_task */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

// Ensure we initialize places:
do_get_profile();

ChromeUtils.defineModuleGetter(this, "PlacesUIUtils",
                               "resource:///modules/PlacesUIUtils.jsm");

// Needed because we rely on the app name for the OPML file name and title.
ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "LiveBookmarkMigratorTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

const {PlacesUtils} =
  ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm", {});
const {LiveBookmarkMigrator} =
  ChromeUtils.import("resource:///modules/LiveBookmarkMigrator.jsm", {});

Cu.importGlobalProperties(["URL"]);

const kTestData = {
  guid: PlacesUtils.bookmarks.toolbarGuid,
  children: [
    {type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: "BM 1", url: new URL("https://example.com/1")},
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "LB 1"},
    {type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: "BM 2", url: new URL("https://example.com/2")},
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "Regular old folder", children: [
      {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "LB child 1"},
      {type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: "BM child 1", url: new URL("https://example.com/1/1")},
      {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "LB child 2"},
      {type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: "BM child 2", url: new URL("https://example.com/1/2")},
    ]},
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "LB 2"},
    {type: PlacesUtils.bookmarks.TYPE_BOOKMARK, title: "BM 3", url: new URL("https://example.com/3")},
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "LB 3"},
    // The next two will be skipped because we ignore the feeds we used to ship by default:
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "Default live bookmark 4"},
    {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: "Default live bookmark 5"},
  ],
};

const kDefaultFeedURLsToTest = ["http://en-US.fxfeeds.mozilla.com/", "http://fxfeeds.mozilla.org/"];

registerCleanupFunction(async () => {
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function setup() {
  let insertedItems = await PlacesUtils.bookmarks.insertTree(kTestData);
  for (let item of insertedItems) {
    if (item.title.includes("LB")) {
      let id = await PlacesUtils.promiseItemId(item.guid);
      let feedID = item.title.replace(/[^0-9]/g, "");
      PlacesUtils.annotations.setItemAnnotation(id, PlacesUtils.LMANNO_FEEDURI,
                                "https://example.com/feed/" + feedID,
                                0, PlacesUtils.annotations.EXPIRE_NEVER,
                                PlacesUtils.bookmarks.SOURCES.DEFAULT, true);
      // For half the feeds, set a site URI as well.
      if (parseInt(feedID) % 2) {
        PlacesUtils.annotations.setItemAnnotation(id, PlacesUtils.LMANNO_SITEURI,
                                  "https://example.com/feedpage/" + feedID,
                                  0, PlacesUtils.annotations.EXPIRE_NEVER,
                                  PlacesUtils.bookmarks.SOURCES.DEFAULT, true);
      }
    } else if (item.title.includes("Default live bookmark")) {
      let id = await PlacesUtils.promiseItemId(item.guid);
      PlacesUtils.annotations.setItemAnnotation(id, PlacesUtils.LMANNO_FEEDURI,
                                kDefaultFeedURLsToTest.pop(),
                                0, PlacesUtils.annotations.EXPIRE_NEVER,
                                PlacesUtils.bookmarks.SOURCES.DEFAULT, true);
    }
  }
});

// Test fetching LB with and without siteURIs works. Check ordering is last-positioned-first.
add_task(async function check_fetches_livemarks_correctly() {
  let detectedLiveBookmarks = await LiveBookmarkMigrator._fetch();
  Assert.equal(detectedLiveBookmarks.length, 7, "Should have 7 live bookmarks");
  Assert.notEqual(detectedLiveBookmarks.filter(lb => lb.siteURI).length, 0,
                  "Should have more than 0 items with a siteURI");
  Assert.notEqual(detectedLiveBookmarks.filter(lb => !lb.siteURI).length, 0,
                  "Should have more than 0 items without a siteURI");
  Assert.equal(detectedLiveBookmarks.filter(lb => lb.feedURI).length, 7,
               "All of them should have a feed URI");
  info(JSON.stringify(detectedLiveBookmarks));
  Assert.deepEqual(detectedLiveBookmarks.map(bm => bm.index), [8, 7, 6, 4, 2, 1, 0],
                   "Should have sorted the items correctly.");
});

// Test that we write sane OPML for some live bookmarks.
add_task(async function check_opml_writing() {
  let bsPass = ChromeUtils.import("resource:///modules/LiveBookmarkMigrator.jsm", {});
  let oldFile = bsPass.OS.File;
  registerCleanupFunction(() => { bsPass.OS.File = oldFile; });

  let lastWriteArgs = null;
  bsPass.OS.File = {
    writeAtomic() {
      lastWriteArgs = Array.from(arguments);
    },
    openUnique() {
      return {file: {close() {}}, path: "foopy.opml"};
    },
  };
  let livemarks = [
    {feedURI: new URL("https://example.com/feed"), siteURI: new URL("https://example.com/"), title: "My funky feed"},
    {feedURI: new URL("https://example.com/otherfeed"), title: "My other feed"},
  ];
  await LiveBookmarkMigrator._writeOPML(livemarks);
  let expectedOPML = `<?xml version="1.0"?>
<opml version="1.0"><head><title>LiveBookmarkMigratorTest Live Bookmarks</title></head><body>
<outline type="rss" title="My funky feed" text="My funky feed" xmlUrl="https://example.com/feed" htmlUrl="https://example.com/"/>
<outline type="rss" title="My other feed" text="My other feed" xmlUrl="https://example.com/otherfeed"/>
</body></opml>`;
  Assert.deepEqual(lastWriteArgs, ["foopy.opml", expectedOPML, {encoding: "utf-8"}], "Should have tried to write the right thing.");
});

// Check all the live bookmarks have been removed, and where they have site URIs, replaced:
add_task(async function check_replace_bookmarks_correctly() {
  let oldLiveBookmarks = await LiveBookmarkMigrator._fetch();
  await LiveBookmarkMigrator._transformBookmarks(oldLiveBookmarks);

  // Check all the remaining bookmarks are not live bookmarks anymore:
  let newBookmarkState = await PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.toolbarGuid);
  function checkNotALiveBookmark(bm) {
    Assert.ok(!bm.annos, "Bookmark " + bm.title + " shouldn't be a live bookmark");
  }
  function iterate(node, fn) {
    fn(node);
    if (node.children) {
      node.children.forEach(fn);
    }
  }
  iterate(newBookmarkState, checkNotALiveBookmark);
  Assert.equal((await LiveBookmarkMigrator._fetch()).length, 0, "Migrator should no longer find live bookmarks");

  info(JSON.stringify(newBookmarkState));
  // Check that all the old live bookmarks have been removed, and where appropriate, replaced:
  for (let bm of oldLiveBookmarks) {
    let replacements = await PlacesUtils.bookmarks.search({title: bm.title});
    let replacement = replacements && replacements[0];
    if (!bm.title.includes("Default live bookmark")) {
      if (replacement) {
        Assert.equal(bm.title, replacement.title, "Found item should have same title");
        Assert.equal(bm.siteURI || bm.feedURI, replacement.url.href, "Found item should have matching URL");
        Assert.equal(bm.index, replacement.index, "Found item should have same position");
        Assert.equal(bm.parentGuid, replacement.parentGuid, "Found item should have same parent");
      } else {
        Assert.ok(false, "Oy! No replacement for `" + bm.title + "` but we expected one!");
      }
    } else {
      Assert.ok(!replacement, "Shouldn't have kept the default bookmark items");
    }
  }
});

