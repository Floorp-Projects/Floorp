/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around ROOT_NODE

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

/**
 * This first test is a simplified version of
 * devtools/server/tests/browser/browser_inspector_walker_watch_root_node.js
 *
 * The original test still asserts some scenarios using several watchRootNode
 * call sites, which is not something we intend to support at the moment in the
 * resource watcher.
 *
 * Otherwise this test checks the basic behavior of the resource when reloading
 * an empty page.
 */
add_task(async function() {
  // Open a test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab("data:text/html,Root Node tests");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  const browser = gBrowser.selectedBrowser;

  info("Call watch([ROOT_NODE], ...)");
  let onAvailableCounter = 0;
  const onAvailable = () => onAvailableCounter++;
  await resourceWatcher.watch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);

  info("Wait until onAvailable has been called");
  await waitUntil(() => onAvailableCounter === 1);
  is(onAvailableCounter, 1, "onAvailable has been called 1 time");

  info("Reload the selected browser");
  browser.reload();

  info("Wait until the watch([ROOT_NODE], ...) callback has been called");
  await waitUntil(() => onAvailableCounter === 2);

  is(onAvailableCounter, 2, "onAvailable has been called 2 times");

  info("Call unwatch([ROOT_NODE], ...) for the onAvailable callback");
  resourceWatcher.unwatch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);

  info("Reload the selected browser");
  const reloaded = BrowserTestUtils.browserLoaded(browser);
  browser.reload();
  await reloaded;

  is(onAvailableCounter, 2, "onAvailable was not called after calling unwatch");

  // Cleanup
  targetList.stopListening();
  await client.close();
});

/**
 * Test that the watchRootNode API provides the expected node fronts.
 */
add_task(async function testRootNodeFrontIsCorrect() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab("data:text/html,<div id=div1>");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);
  const browser = gBrowser.selectedBrowser;

  info("Call watch([ROOT_NODE], ...)");

  let rootNodeResolve;
  let rootNodePromise = new Promise(r => (rootNodeResolve = r));
  const onAvailable = rootNodeFront => rootNodeResolve(rootNodeFront);
  await resourceWatcher.watch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);

  info("Wait until onAvailable has been called");
  const { resource: root1, resourceType } = await rootNodePromise;
  ok(!!root1, "onAvailable has been called with a valid argument");
  is(
    resourceType,
    ResourceWatcher.TYPES.ROOT_NODE,
    "The resource has the expected type"
  );

  info("Check we can query an expected node under the retrieved root");
  const div1 = await root1.walkerFront.querySelector(root1, "div");
  is(div1.getAttribute("id"), "div1", "Correct root node retrieved");

  info("Reload the selected browser");
  rootNodePromise = new Promise(r => (rootNodeResolve = r));
  browser.reload();

  const { resource: root2 } = await rootNodePromise;
  ok(
    root1 !== root2,
    "onAvailable has been called with a different node front after reload"
  );

  info("Navigate to another URL");
  rootNodePromise = new Promise(r => (rootNodeResolve = r));
  BrowserTestUtils.loadURI(browser, `data:text/html,<div id=div3>`);
  const { resource: root3 } = await rootNodePromise;
  info("Check we can query an expected node under the retrieved root");
  const div3 = await root3.walkerFront.querySelector(root3, "div");
  is(div3.getAttribute("id"), "div3", "Correct root node retrieved");

  // Cleanup
  resourceWatcher.unwatch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);
  targetList.stopListening();
  await client.close();
});
