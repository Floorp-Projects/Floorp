/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

/**
 * Check that the resource watcher is still properly watching for new targets
 * after unwatching one resource, if there is still another watched resource.
 */
add_task(async function() {
  // Open a test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab("data:text/html,Root Node tests");

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget();

  const { CONSOLE_MESSAGES, ROOT_NODE } = ResourceWatcher.TYPES;

  // We are only interested in console messages as a resource, the ROOT_NODE one
  // is here to test the ResourceWatcher::unwatch API with several resources.
  let receivedMessages = 0;
  const onAvailable = ({ resourceType }) => {
    if (resourceType === CONSOLE_MESSAGES) {
      receivedMessages++;
    }
  };

  info("Call watch([CONSOLE_MESSAGES, ROOT_NODE], ...)");
  await resourceWatcher.watch([CONSOLE_MESSAGES, ROOT_NODE], onAvailable);

  info("Use console.log in the content page");
  logInTab(tab, "test");
  info("Wait until onAvailable received 1 resource of type CONSOLE_MESSAGES");
  await waitUntil(() => receivedMessages === 1);

  // Check that the resource watcher captures resources from new targets.
  info("Open a first tab on the example.com domain");
  const comTab = await addTab(
    "http://example.com/document-builder.sjs?html=com"
  );
  info("Use console.log in the example.com page");
  logInTab(comTab, "test-from-example-com");
  info("Wait until onAvailable received 2 resources of type CONSOLE_MESSAGES");
  await waitUntil(() => receivedMessages === 2);

  info("Stop watching ROOT_NODE resources");
  await resourceWatcher.unwatch([ROOT_NODE], onAvailable);

  // Check that messages from new targets are still captured after calling
  // unwatch for another resource.
  info("Open a second tab on the example.net domain");
  const netTab = await addTab(
    "http://example.net/document-builder.sjs?html=net"
  );
  info("Use console.log in the example.net page");
  logInTab(netTab, "test-from-example-net");
  info("Wait until onAvailable received 3 resources of type CONSOLE_MESSAGES");
  await waitUntil(() => receivedMessages === 3);

  info("Stop watching CONSOLE_MESSAGES resources");
  await resourceWatcher.unwatch([CONSOLE_MESSAGES], onAvailable);
  await logInTab(tab, "test-again");

  // We don't have a specific event to wait for here, so allow some time for
  // the message to be received.
  await wait(1000);

  is(
    receivedMessages,
    3,
    "The resource watcher should not watch CONSOLE_MESSAGES anymore"
  );

  // Cleanup
  targetList.stopListening();
  await client.close();
});

function logInTab(tab, message) {
  return ContentTask.spawn(tab.linkedBrowser, message, function(_message) {
    content.console.log(_message);
  });
}
