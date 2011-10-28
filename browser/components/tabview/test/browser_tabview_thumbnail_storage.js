/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tests = [testRawSyncSave, testRawAsyncSave, testRawLoadError,
             testAsyncSave, testSyncSave, testOverrideAsyncSave,
             testSaveCleanThumbnail];

function test() {
  waitForExplicitFinish();
  loadTabView(next);
}

function testRawSyncSave() {
  let cw = TabView.getContentWindow();
  let url = "http://example.com/sync-url";
  let data = "thumbnail-data-sync";
  let saved = false;

  cw.ThumbnailStorage.saveThumbnail(url, data, function (error) {
    ok(!error, "thumbnail entry was saved");
    ok(!saved, "thumbnail was saved synchronously");

    cw.ThumbnailStorage.loadThumbnail(url, function (error, imageData) {
      ok(!error, "thumbnail entry was loaded");
      is(imageData, data, "valid thumbnail data received");
      next();
    });
  }, {synchronously: true});

  saved = true;
}

function testRawAsyncSave() {
  let cw = TabView.getContentWindow();
  let url = "http://example.com/async-url";
  let data = "thumbnail-data-async";
  let saved = false;

  cw.ThumbnailStorage.saveThumbnail(url, data, function (error) {
    ok(!error, "thumbnail entry was saved");
    ok(saved, "thumbnail was saved asynchronously");

    cw.ThumbnailStorage.loadThumbnail(url, function (error, imageData) {
      ok(!error, "thumbnail entry was loaded");
      is(imageData, data, "valid thumbnail data received");
      next();
    });
  });

  saved = true;
}

function testRawLoadError() {
  let cw = TabView.getContentWindow();

  cw.ThumbnailStorage.loadThumbnail("non-existant-url", function (error, data) {
    ok(error, "thumbnail entry failed to load");
    is(null, data, "no thumbnail data received");
    next();
  });
}

function testSyncSave() {
  let tabItem = gBrowser.tabs[0]._tabViewTabItem;

  // set the thumbnail to dirty
  tabItem.tabCanvas.paint();

  let saved = false;

  whenThumbnailSaved(tabItem, function () {
    ok(!saved, "thumbnail was saved synchronously");
    next();
  });

  tabItem.saveThumbnail({synchronously: true});
  saved = true;
}

function testAsyncSave() {
  let tabItem = gBrowser.tabs[0]._tabViewTabItem;

  // set the thumbnail to dirty
  tabItem.tabCanvas.paint();

  let saved = false;

  whenThumbnailSaved(tabItem, function () {
    ok(saved, "thumbnail was saved asynchronously");
    next();
  });

  tabItem.saveThumbnail();
  saved = true;
}

function testOverrideAsyncSave() {
  let tabItem = gBrowser.tabs[0]._tabViewTabItem;

  // set the thumbnail to dirty
  tabItem.tabCanvas.paint();

  // initiate async save
  tabItem.saveThumbnail();

  let saveCount = 0;

  whenThumbnailSaved(tabItem, function () {
    saveCount = 1;
  });

  tabItem.saveThumbnail({synchronously: true});

  is(saveCount, 1, "thumbnail got saved once");
  next();
}

function testSaveCleanThumbnail() {
  let tabItem = gBrowser.tabs[0]._tabViewTabItem;

  // set the thumbnail to dirty
  tabItem.tabCanvas.paint();

  let saveCount = 0;

  whenThumbnailSaved(tabItem, function () saveCount++);
  tabItem.saveThumbnail({synchronously: true});
  tabItem.saveThumbnail({synchronously: true});

  is(saveCount, 1, "thumbnail got saved once, only");
  next();
}

// ----------
function whenThumbnailSaved(tabItem, callback) {
  tabItem.addSubscriber("savedCachedImageData", function onSaved() {
    tabItem.removeSubscriber("savedCachedImageData", onSaved);
    callback();
  });
}

// ----------
function loadTabView(callback) {
  afterAllTabsLoaded(function () {
    showTabView(function () {
      hideTabView(callback);
    });
  });
}

// ----------
function next() {
  let test = tests.shift();

  if (test) {
    info("* running " + test.name + "...");
    test();
  } else {
    finish();
  }
}
