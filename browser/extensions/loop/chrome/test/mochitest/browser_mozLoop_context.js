/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for various context-in-conversations helpers in
 * LoopUI and Loop API.
 */
"use strict";

var [, gHandlers] = LoopAPI.inspect();

function promiseGetMetadata() {
  return new Promise(resolve => gHandlers.GetSelectedTabMetadata({}, resolve));
}

add_task(function* test_mozLoop_getSelectedTabMetadata() {
  let metadata = yield promiseGetMetadata();

  Assert.strictEqual(metadata.url, null, "URL should be empty for about:blank");
  Assert.strictEqual(metadata.favicon, null, "Favicon should be empty for about:blank");
  Assert.strictEqual(metadata.title, "", "Title should be empty for about:blank");
  Assert.deepEqual(metadata.previews, [], "No previews available for about:blank");

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  metadata = yield promiseGetMetadata();

  Assert.strictEqual(metadata.url, null, "URL should be empty for about:home");
  Assert.strictEqual(metadata.favicon, null, "Favicon should be empty for about:home");
  Assert.ok(metadata.title, "Title should be set for about:home");
  // Filter out null elements in the previews - contentSearchUI adds some img
  // elements with chrome:// srcs, which show up as null in metadata.previews.
  Assert.deepEqual(metadata.previews.filter(e => e), [], "No previews available for about:home");

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_mozLoop_getSelectedTabMetadata_defaultIcon() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  let metadata = yield promiseGetMetadata();

  Assert.strictEqual(metadata.url, "http://example.com/", "URL should match");
  Assert.strictEqual(metadata.favicon, null, "Favicon should be empty");
  Assert.ok(metadata.title, "Title should be set");
  Assert.deepEqual(metadata.previews, [], "No previews available");

  yield BrowserTestUtils.removeTab(tab);
});
