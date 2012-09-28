/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/?t=" + Date.now();
const URL1 = URL + "#1";
const URL2 = URL + "#2";
const URL3 = URL + "#3";

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

let tmp = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("resource:///modules/PageThumbs.jsm", tmp);

const {EXPIRATION_MIN_CHUNK_SIZE, PageThumbsExpiration} = tmp;

function runTests() {
  // Create three thumbnails.
  createDummyThumbnail(URL1);
  ok(thumbnailExists(URL1), "first thumbnail created");

  createDummyThumbnail(URL2);
  ok(thumbnailExists(URL2), "second thumbnail created");

  createDummyThumbnail(URL3);
  ok(thumbnailExists(URL3), "third thumbnail created");

  // Remove the third thumbnail.
  yield expireThumbnails([URL1, URL2]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(thumbnailExists(URL2), "second thumbnail still exists");
  ok(!thumbnailExists(URL3), "third thumbnail has been removed");

  // Remove the second thumbnail.
  yield expireThumbnails([URL1]);
  ok(thumbnailExists(URL1), "first thumbnail still exists");
  ok(!thumbnailExists(URL2), "second thumbnail has been removed");

  // Remove all thumbnails.
  yield expireThumbnails([]);
  ok(!thumbnailExists(URL1), "all thumbnails have been removed");

  // Create some more files than the min chunk size.
  let urls = [];
  for (let i = 0; i < EXPIRATION_MIN_CHUNK_SIZE + 10; i++) {
    urls.push(URL + "#dummy" + i);
  }

  urls.forEach(createDummyThumbnail);
  ok(urls.every(thumbnailExists), "all dummy thumbnails created");

  // Expire thumbnails and expect 10 remaining.
  yield expireThumbnails([]);
  let remainingURLs = [u for (u of urls) if (thumbnailExists(u))];
  is(remainingURLs.length, 10, "10 dummy thumbnails remaining");

  // Expire thumbnails again. All should be gone by now.
  yield expireThumbnails([]);
  remainingURLs = [u for (u of remainingURLs) if (thumbnailExists(u))];
  is(remainingURLs.length, 0, "no dummy thumbnails remaining");
}

function createDummyThumbnail(aURL) {
  let file = PageThumbsStorage.getFileForURL(aURL);
  let fos = FileUtils.openSafeFileOutputStream(file);

  let data = "dummy";
  fos.write(data, data.length);
  FileUtils.closeSafeFileOutputStream(fos);
}

function expireThumbnails(aKeep) {
  PageThumbsExpiration.expireThumbnails(aKeep, function () {
    executeSoon(next);
  });
}
