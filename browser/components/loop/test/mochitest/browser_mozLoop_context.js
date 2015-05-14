/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for various context-in-conversations helpers in
 * LoopUI and MozLoopAPI.
 */
"use strict";

add_task(loadLoopPanel);

function promiseGetMetadata() {
  return new Promise(resolve => gMozLoopAPI.getSelectedTabMetadata(resolve));
}

add_task(function* test_mozLoop_getSelectedTabMetadata() {
  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  let metadata = yield promiseGetMetadata();

  Assert.strictEqual(metadata.url, null, "URL should be empty for about:blank");
  Assert.strictEqual(metadata.favicon, null, "Favicon should be empty for about:blank");
  Assert.strictEqual(metadata.title, "", "Title should be empty for about:blank");
  Assert.deepEqual(metadata.previews, [], "No previews available for about:blank");

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, "about:home");
  metadata = yield promiseGetMetadata();

  Assert.strictEqual(metadata.url, null, "URL should be empty for about:home");
  Assert.ok(metadata.favicon.startsWith("data:image/x-icon;base64,"),
    "Favicon should be set for about:home");
  Assert.ok(metadata.title, "Title should be set for about:home");
  Assert.deepEqual(metadata.previews, [], "No previews available for about:home");

  gBrowser.removeTab(tab);
});
