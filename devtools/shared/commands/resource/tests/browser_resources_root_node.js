/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around ROOT_NODE

/**
 * The original test still asserts some scenarios using several watchRootNode
 * call sites, which is not something we intend to support at the moment in the
 * resource command.
 *
 * Otherwise this test checks the basic behavior of the resource when reloading
 * an empty page.
 */
add_task(async function () {
  // Open a test tab
  const tab = await addTab("data:text/html,Root Node tests");

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  const browser = gBrowser.selectedBrowser;

  info("Call watchResources([ROOT_NODE], ...)");
  let onAvailableCounter = 0;
  const onAvailable = resources => (onAvailableCounter += resources.length);
  await resourceCommand.watchResources([resourceCommand.TYPES.ROOT_NODE], {
    onAvailable,
  });

  info("Wait until onAvailable has been called");
  await waitUntil(() => onAvailableCounter === 1);
  is(onAvailableCounter, 1, "onAvailable has been called 1 time");

  info("Reload the selected browser");
  browser.reload();

  info(
    "Wait until the watchResources([ROOT_NODE], ...) callback has been called"
  );
  await waitUntil(() => onAvailableCounter === 2);

  is(onAvailableCounter, 2, "onAvailable has been called 2 times");

  info("Call unwatchResources([ROOT_NODE], ...) for the onAvailable callback");
  resourceCommand.unwatchResources([resourceCommand.TYPES.ROOT_NODE], {
    onAvailable,
  });

  info("Reload the selected browser");
  const reloaded = BrowserTestUtils.browserLoaded(browser);
  browser.reload();
  await reloaded;

  is(
    onAvailableCounter,
    2,
    "onAvailable was not called after calling unwatchResources"
  );

  // Cleanup
  targetCommand.destroy();
  await client.close();
});

/**
 * Test that the watchRootNode API provides the expected node fronts.
 */
add_task(async function testRootNodeFrontIsCorrect() {
  const tab = await addTab("data:text/html,<div id=div1>");

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );
  const browser = gBrowser.selectedBrowser;

  info("Call watchResources([ROOT_NODE], ...)");

  let rootNodeResolve;
  let rootNodePromise = new Promise(r => (rootNodeResolve = r));
  const onAvailable = ([rootNodeFront]) => rootNodeResolve(rootNodeFront);
  await resourceCommand.watchResources([resourceCommand.TYPES.ROOT_NODE], {
    onAvailable,
  });

  info("Wait until onAvailable has been called");
  const root1 = await rootNodePromise;
  ok(!!root1, "onAvailable has been called with a valid argument");
  is(
    root1.resourceType,
    resourceCommand.TYPES.ROOT_NODE,
    "The resource has the expected type"
  );

  info("Check we can query an expected node under the retrieved root");
  const div1 = await root1.walkerFront.querySelector(root1, "div");
  is(div1.getAttribute("id"), "div1", "Correct root node retrieved");

  info("Reload the selected browser");
  rootNodePromise = new Promise(r => (rootNodeResolve = r));
  browser.reload();

  const root2 = await rootNodePromise;
  ok(
    root1 !== root2,
    "onAvailable has been called with a different node front after reload"
  );

  info("Navigate to another URL");
  rootNodePromise = new Promise(r => (rootNodeResolve = r));
  BrowserTestUtils.startLoadingURIString(
    browser,
    `data:text/html,<div id=div3>`
  );
  const root3 = await rootNodePromise;
  info("Check we can query an expected node under the retrieved root");
  const div3 = await root3.walkerFront.querySelector(root3, "div");
  is(div3.getAttribute("id"), "div3", "Correct root node retrieved");

  // Cleanup
  resourceCommand.unwatchResources([resourceCommand.TYPES.ROOT_NODE], {
    onAvailable,
  });
  targetCommand.destroy();
  await client.close();
});
