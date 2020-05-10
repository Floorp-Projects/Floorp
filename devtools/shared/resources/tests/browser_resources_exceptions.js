/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API exceptions when using unsupported patterns

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  // Open a test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab("data:text/html,ResourceWatcher exception tests");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Call watch once");
  const onAvailable = () => {};
  await resourceWatcher.watch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);

  info("Call watch again, should throw because we are already listening");
  const expectedMessage1 =
    `The ResourceWatcher is already listening to "${ResourceWatcher.TYPES.ROOT_NODE}", ` +
    "the client should call `watch` only once per resource type.";

  await Assert.rejects(
    resourceWatcher.watch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable),
    err => err.message === expectedMessage1
  );

  info("Call unwatch");
  resourceWatcher.unwatch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable);

  info("Call watch again, should throw because we already listened previously");
  const expectedMessage2 =
    `The ResourceWatcher previously watched "${ResourceWatcher.TYPES.ROOT_NODE}" ` +
    "and doesn't support watching again on a previous resource.";

  await Assert.rejects(
    resourceWatcher.watch([ResourceWatcher.TYPES.ROOT_NODE], onAvailable),
    err => err.message === expectedMessage2
  );

  targetList.stopListening();
  await client.close();
});
