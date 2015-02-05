/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = HTTPROOT + "browser_frametree_sample.html";
const URL_FRAMESET = HTTPROOT + "browser_frametree_sample_frameset.html";

/**
 * This ensures that loading a page normally, aborting a page load, reloading
 * a page, navigating using the bfcache, and ignoring frames that were
 * created dynamically work as expect. We expect the frame tree to be reset
 * when a page starts loading and we also expect a valid frame tree to exist
 * when it has stopped loading.
 */
add_task(function test_frametree() {
  const FRAME_TREE_SINGLE = { href: URL };
  const FRAME_TREE_FRAMESET = {
    href: URL_FRAMESET,
    children: [{href: URL}, {href: URL}, {href: URL}]
  };

  // Create a tab with a single frame.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseNewFrameTree(browser);
  yield checkFrameTree(browser, FRAME_TREE_SINGLE,
    "loading a page resets and creates the frame tree correctly");

  // Load the frameset and create two frames dynamically, the first on
  // DOMContentLoaded and the second on load.
  yield sendMessage(browser, "ss-test:createDynamicFrames", {id: "frames", url: URL});
  browser.loadURI(URL_FRAMESET);
  yield promiseNewFrameTree(browser);
  yield checkFrameTree(browser, FRAME_TREE_FRAMESET,
    "dynamic frames created on or after the load event are ignored");

  // Go back to the previous single-frame page. There will be no load event as
  // the page is still in the bfcache. We thus make sure this type of navigation
  // resets the frame tree.
  browser.goBack();
  yield promiseNewFrameTree(browser);
  yield checkFrameTree(browser, FRAME_TREE_SINGLE,
    "loading from bfache resets and creates the frame tree correctly");

  // Load the frameset again but abort the load early.
  // The frame tree should still be reset and created.
  browser.loadURI(URL_FRAMESET);
  executeSoon(() => browser.stop());
  yield promiseNewFrameTree(browser);

  // Load the frameset and check the tree again.
  yield sendMessage(browser, "ss-test:createDynamicFrames", {id: "frames", url: URL});
  browser.loadURI(URL_FRAMESET);
  yield promiseNewFrameTree(browser);
  yield checkFrameTree(browser, FRAME_TREE_FRAMESET,
    "reloading a page resets and creates the frame tree correctly");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * This test ensures that we ignore frames that were created dynamically at or
 * after the load event. SessionStore can't handle these and will not restore
 * or collect any data for them.
 */
add_task(function test_frametree_dynamic() {
  // The frame tree as expected. The first two frames are static
  // and the third one was created on DOMContentLoaded.
  const FRAME_TREE = {
    href: URL_FRAMESET,
    children: [{href: URL}, {href: URL}, {href: URL}]
  };
  const FRAME_TREE_REMOVED = {
    href: URL_FRAMESET,
    children: [{href: URL}, {href: URL}]
  };

  // Add an empty tab for a start.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Create dynamic frames on "DOMContentLoaded" and on "load".
  yield sendMessage(browser, "ss-test:createDynamicFrames", {id: "frames", url: URL});
  browser.loadURI(URL_FRAMESET);
  yield promiseNewFrameTree(browser);

  // Check that the frame tree does not contain the frame created on "load".
  // The two static frames and the one created on DOMContentLoaded must be in
  // the tree.
  yield checkFrameTree(browser, FRAME_TREE,
    "frame tree contains first four frames");

  // Remove the last frame in the frameset.
  yield sendMessage(browser, "ss-test:removeLastFrame", {id: "frames"});
  // Check that the frame tree didn't change.
  yield checkFrameTree(browser, FRAME_TREE,
    "frame tree contains first four frames");

  // Remove the last frame in the frameset.
  yield sendMessage(browser, "ss-test:removeLastFrame", {id: "frames"});
  // Check that the frame tree excludes the removed frame.
  yield checkFrameTree(browser, FRAME_TREE_REMOVED,
    "frame tree contains first three frames");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Checks whether the current frame hierarchy of a given |browser| matches the
 * |expected| frame hierarchy.
 */
function checkFrameTree(browser, expected, msg) {
  return sendMessage(browser, "ss-test:mapFrameTree").then(tree => {
    is(JSON.stringify(tree), JSON.stringify(expected), msg);
  });
}

/**
 * Returns a promise that will be resolved when the given |browser| has loaded
 * and we received messages saying that its frame tree has been reset and
 * recollected.
 */
function promiseNewFrameTree(browser) {
  let reset = promiseContentMessage(browser, "ss-test:onFrameTreeCollected");
  let collect = promiseContentMessage(browser, "ss-test:onFrameTreeCollected");
  return Promise.all([reset, collect]);
}
