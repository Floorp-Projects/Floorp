/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the resource watcher is still properly watching for new targets
 * after unwatching one resource, if there is still another watched resource.
 */
add_task(async function() {
  // We will create a main process target list here in order to monitor
  // resources from new tabs as they get created.
  // devtools.browsertoolbox.fission should be true to monitor resources from
  // remote browsers & frames.
  await pushPref("devtools.browsertoolbox.fission", true);

  // Open a test tab
  const tab = await addTab("data:text/html,Root Node tests");

  const {
    client,
    resourceWatcher,
    targetCommand,
  } = await initMultiProcessResourceWatcher();

  const { CONSOLE_MESSAGE, ROOT_NODE } = resourceWatcher.TYPES;

  // We are only interested in console messages as a resource, the ROOT_NODE one
  // is here to test the ResourceWatcher::unwatchResources API with several resources.
  const receivedMessages = [];
  const onAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType === CONSOLE_MESSAGE) {
        receivedMessages.push(resource);
      }
    }
  };

  info("Call watchResources([CONSOLE_MESSAGE, ROOT_NODE], ...)");
  await resourceWatcher.watchResources([CONSOLE_MESSAGE, ROOT_NODE], {
    onAvailable,
  });

  info("Use console.log in the content page");
  logInTab(tab, "test from data-url");
  info(
    "Wait until onAvailable received the CONSOLE_MESSAGE resource emitted from the data-url tab"
  );
  await waitUntil(() =>
    receivedMessages.find(
      resource => resource.message.arguments[0] === "test from data-url"
    )
  );

  // Check that the resource watcher captures resources from new targets.
  info("Open a first tab on the example.com domain");
  const comTab = await addTab(
    "http://example.com/document-builder.sjs?html=com"
  );
  info("Use console.log in the example.com page");
  logInTab(comTab, "test-from-example-com");
  info(
    "Wait until onAvailable received the CONSOLE_MESSAGE resource emitted from the example.com tab"
  );
  await waitUntil(() =>
    receivedMessages.find(
      resource => resource.message.arguments[0] === "test-from-example-com"
    )
  );

  info("Stop watching ROOT_NODE resources");
  await resourceWatcher.unwatchResources([ROOT_NODE], { onAvailable });

  // Check that messages from new targets are still captured after calling
  // unwatch for another resource.
  info("Open a second tab on the example.net domain");
  const netTab = await addTab(
    "http://example.net/document-builder.sjs?html=net"
  );
  info("Use console.log in the example.net page");
  logInTab(netTab, "test-from-example-net");
  info(
    "Wait until onAvailable received the CONSOLE_MESSAGE resource emitted from the example.net tab"
  );
  await waitUntil(() =>
    receivedMessages.find(
      resource => resource.message.arguments[0] === "test-from-example-net"
    )
  );

  info("Stop watching CONSOLE_MESSAGE resources");
  await resourceWatcher.unwatchResources([CONSOLE_MESSAGE], { onAvailable });
  await logInTab(tab, "test-again");

  // We don't have a specific event to wait for here, so allow some time for
  // the message to be received.
  await wait(1000);

  is(
    receivedMessages.find(
      resource => resource.message.arguments[0] === "test-again"
    ),
    undefined,
    "The resource watcher should not watch CONSOLE_MESSAGE anymore"
  );

  // Cleanup
  targetCommand.destroy();
  await client.close();
});

function logInTab(tab, message) {
  return ContentTask.spawn(tab.linkedBrowser, message, function(_message) {
    content.console.log(_message);
  });
}
