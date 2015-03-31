/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/readinglist/ReadingList.jsm");
Cu.import("resource:///modules/readinglist/SQLiteStore.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");

var gStore;
var gItems;

function run_test() {
  run_next_test();
}

add_task(function* prepare() {
  let basename = "reading-list-test.sqlite";
  let dbFile = do_get_profile();
  dbFile.append(basename);
  function removeDB() {
    if (dbFile.exists()) {
      dbFile.remove(true);
    }
  }
  removeDB();
  do_register_cleanup(function* () {
    // Wait for the store to close its connection to the database.
    yield gStore.destroy();
    removeDB();
  });

  gStore = new SQLiteStore(dbFile.path);

  gItems = [];
  for (let i = 0; i < 3; i++) {
    gItems.push({
      guid: `guid${i}`,
      url: `http://example.com/${i}`,
      resolvedURL: `http://example.com/resolved/${i}`,
      title: `title ${i}`,
      excerpt: `excerpt ${i}`,
      unread: true,
      addedOn: i,
    });
  }

  for (let item of gItems) {
    yield gStore.addItem(item);
  }
});

add_task(function* constraints() {
  // add an item again
  let err = null;
  try {
    yield gStore.addItem(gItems[0]);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists:") >= 0);

  // add a new item with an existing guid
  function kindOfClone(item) {
    let newItem = {};
    for (let prop in item) {
      newItem[prop] = item[prop];
      if (typeof(newItem[prop]) == "string") {
        newItem[prop] += " -- make this string different";
      }
    }
    return newItem;
  }
  let item = kindOfClone(gItems[0]);
  item.guid = gItems[0].guid;
  err = null;
  try {
    yield gStore.addItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists: guid") >= 0);

  // add a new item with an existing url
  item = kindOfClone(gItems[0]);
  item.url = gItems[0].url;
  err = null;
  try {
    yield gStore.addItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists: url") >= 0);

  // update an item with an existing url
  item.guid = gItems[1].guid;
  err = null;
  try {
    yield gStore.updateItem(item);
  }
  catch (e) {
    err = e;
  }
  // The failure actually happens on items.guid, not items.url, because the item
  // is first looked up by url, and then its other properties are updated on the
  // resulting row.
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists: guid") >= 0);

  // add a new item with an existing resolvedURL
  item = kindOfClone(gItems[0]);
  item.resolvedURL = gItems[0].resolvedURL;
  err = null;
  try {
    yield gStore.addItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists: resolvedURL") >= 0);

  // update an item with an existing resolvedURL
  item.url = gItems[1].url;
  err = null;
  try {
    yield gStore.updateItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(err);
  Assert.ok(err instanceof ReadingList.Error.Exists);
  Assert.ok(err.message);
  Assert.ok(err.message.indexOf("An item with the following property already exists: resolvedURL") >= 0);

  // add a new item with no guid, which is allowed
  item = kindOfClone(gItems[0]);
  delete item.guid;
  err = null;
  try {
    yield gStore.addItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(!err, err ? err.message : undefined);
  let url1 = item.url;

  // add a second new item with no guid, which is allowed
  item = kindOfClone(gItems[1]);
  delete item.guid;
  err = null;
  try {
    yield gStore.addItem(item);
  }
  catch (e) {
    err = e;
  }
  Assert.ok(!err, err ? err.message : undefined);
  let url2 = item.url;

  // Delete both items since other tests assume the store contains only gItems.
  yield gStore.deleteItemByURL(url1);
  yield gStore.deleteItemByURL(url2);
  let items = [];
  yield gStore.forEachItem(i => items.push(i), [{ url: [url1, url2] }]);
  Assert.equal(items.length, 0);
});

add_task(function* count() {
  let count = yield gStore.count();
  Assert.equal(count, gItems.length);

  count = yield gStore.count([{
    guid: gItems[0].guid,
  }]);
  Assert.equal(count, 1);
});

add_task(function* forEachItem() {
  // all items
  let items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    sort: "guid",
  }]);
  checkItems(items, gItems);

  // first item
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    limit: 1,
    sort: "guid",
  }]);
  checkItems(items, gItems.slice(0, 1));

  // last item
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    limit: 1,
    sort: "guid",
    descending: true,
  }]);
  checkItems(items, gItems.slice(gItems.length - 1, gItems.length));

  // match on a scalar property
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    guid: gItems[0].guid,
  }]);
  checkItems(items, gItems.slice(0, 1));

  // match on an array
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    guid: gItems.map(i => i.guid),
    sort: "guid",
  }]);
  checkItems(items, gItems);

  // match on AND'ed properties
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    guid: gItems.map(i => i.guid),
    title: gItems[0].title,
    sort: "guid",
  }]);
  checkItems(items, [gItems[0]]);

  // match on OR'ed properties
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    guid: gItems[1].guid,
    sort: "guid",
  }, {
    guid: gItems[0].guid,
  }]);
  checkItems(items, [gItems[0], gItems[1]]);

  // match on AND'ed and OR'ed properties
  items = [];
  yield gStore.forEachItem(item => items.push(item), [{
    guid: gItems.map(i => i.guid),
    title: gItems[1].title,
    sort: "guid",
  }, {
    guid: gItems[0].guid,
  }]);
  checkItems(items, [gItems[0], gItems[1]]);
});

add_task(function* updateItem() {
  let newTitle = "a new title";
  gItems[0].title = newTitle;
  yield gStore.updateItem(gItems[0]);
  let item;
  yield gStore.forEachItem(i => item = i, [{
    guid: gItems[0].guid,
  }]);
  Assert.ok(item);
  Assert.equal(item.title, gItems[0].title);
});

add_task(function* updateItemByGUID() {
  let newTitle = "updateItemByGUID";
  gItems[0].title = newTitle;
  yield gStore.updateItemByGUID(gItems[0]);
  let item;
  yield gStore.forEachItem(i => item = i, [{
    guid: gItems[0].guid,
  }]);
  Assert.ok(item);
  Assert.equal(item.title, gItems[0].title);
});

// This test deletes items so it should probably run last.
add_task(function* deleteItemByURL() {
  // delete first item
  yield gStore.deleteItemByURL(gItems[0].url);
  Assert.equal((yield gStore.count()), gItems.length - 1);
  let items = [];
  yield gStore.forEachItem(i => items.push(i), [{
    sort: "guid",
  }]);
  checkItems(items, gItems.slice(1));

  // delete second item
  yield gStore.deleteItemByURL(gItems[1].url);
  Assert.equal((yield gStore.count()), gItems.length - 2);
  items = [];
  yield gStore.forEachItem(i => items.push(i), [{
    sort: "guid",
  }]);
  checkItems(items, gItems.slice(2));
});

// This test deletes items so it should probably run last.
add_task(function* deleteItemByGUID() {
  // delete third item
  yield gStore.deleteItemByGUID(gItems[2].guid);
  Assert.equal((yield gStore.count()), gItems.length - 3);
  let items = [];
  yield gStore.forEachItem(i => items.push(i), [{
    sort: "guid",
  }]);
  checkItems(items, gItems.slice(3));
});

function checkItems(actualItems, expectedItems) {
  Assert.equal(actualItems.length, expectedItems.length);
  for (let i = 0; i < expectedItems.length; i++) {
    for (let prop in expectedItems[i]) {
      Assert.ok(prop in actualItems[i], prop);
      Assert.equal(actualItems[i][prop], expectedItems[i][prop]);
    }
  }
}
