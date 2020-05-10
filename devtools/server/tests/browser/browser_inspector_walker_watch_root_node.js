/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from inspector-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

/**
 * Test the watch/unwatchRootNodeFront API available on the walker front.
 * This first test checks that the callbacks are called the expected number of
 * time.
 */
add_task(async function() {
  const { walker } = await initInspectorFront(`data:text/html,<div>`);
  const browser = gBrowser.selectedBrowser;

  info("Call watchRootNode");
  let onAvailableCounter1 = 0;
  const onAvailable1 = () => onAvailableCounter1++;
  await walker.watchRootNode(onAvailable1);

  info("Wait until onAvailable1 has been called");
  await waitUntil(() => onAvailableCounter1 === 1);
  is(onAvailableCounter1, 1, "onAvailable1 has been called 1 time");

  info("Reload the selected browser");
  browser.reload();

  info("Wait until the watchRootNode callback has been called");
  await waitUntil(() => onAvailableCounter1 === 2);

  is(onAvailableCounter1, 2, "onAvailable1 has been called 2 times");

  info("Call watchNode with only the onAvailable callback");
  let onAvailableCounter2 = 0;
  const onAvailable2 = () => onAvailableCounter2++;
  await walker.watchRootNode(onAvailable2);

  info("Wait until onAvailable2 has been called");
  await waitUntil(() => onAvailableCounter2 === 1);
  is(onAvailableCounter2, 1, "onAvailable2 has been called 1 time");

  info("Reload the selected browser");
  browser.reload();

  info("Wait until the watchRootNode callback has been called");
  await waitUntil(() => {
    return onAvailableCounter1 === 3 && onAvailableCounter2 === 2;
  });

  is(onAvailableCounter1, 3, "onAvailable1 has been called 3 times");
  is(onAvailableCounter2, 2, "onAvailable2 has been called 2 times");

  info("Call unwatchRootNode for the onAvailable2 callback");
  walker.unwatchRootNode(onAvailable2);

  info("Reload the selected browser");
  browser.reload();

  info("Wait until the watchRootNode callback has been called");
  await waitUntil(() => onAvailableCounter1 === 4);

  is(onAvailableCounter1, 4, "onAvailable1 has been called 4 times");
  is(onAvailableCounter2, 2, "onAvailable2 was not called again");

  info("Call unwatchRootNode for the onAvailable1 callback");
  walker.unwatchRootNode(onAvailable1);

  info("Reload the selected browser");
  const reloaded = BrowserTestUtils.browserLoaded(browser);
  browser.reload();
  await reloaded;

  is(onAvailableCounter1, 4, "onAvailable1 was not called again");
  is(onAvailableCounter2, 2, "onAvailable2 was not called again");

  // After removing all watchers, we watch again to check that the front is
  // able to resume watching correctly.
  info("Call watchRootNode for the onAvailable1 callback again");
  await walker.watchRootNode(onAvailable1);

  info("Reload the selected browser");
  browser.reload();
  await waitUntil(() => onAvailableCounter1 === 5);

  is(onAvailableCounter1, 5, "onAvailable1 was called again once");
  is(onAvailableCounter2, 2, "onAvailable2 was not called again");

  // Cleanup.
  walker.unwatchRootNode(onAvailable1);
});

/**
 * Test that the actor will re-emit an already emitted root if we call unwatch
 * and then watch again.
 */
add_task(async function testCallingWatchSuccessivelyWithoutReload() {
  const { walker } = await initInspectorFront(`data:text/html,<div>`);

  info("Call watchRootNode");
  let onAvailableCounter = 0;
  const onAvailable = () => onAvailableCounter++;
  await walker.watchRootNode(onAvailable);

  info("Wait until onAvailable has been called");
  await waitUntil(() => onAvailableCounter === 1);

  info("Unwatch and watch again");
  walker.unwatchRootNode(onAvailable);
  await walker.watchRootNode(onAvailable);

  // The actor already notified about the current root node, but it should
  // do it again since we called unwatch first.
  info("Wait until the callback is called again");
  await waitUntil(() => onAvailableCounter === 2);

  // cleanup
  walker.unwatchRootNode(onAvailable);
});
