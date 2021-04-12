/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that calling unwatchResources before watchResources could resolve still
// removes watcher entries correctly.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const TEST_URI = "data:text/html;charset=utf-8,";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );
  const { CONSOLE_MESSAGE, ROOT_NODE } = ResourceWatcher.TYPES;

  info("Use console.log in the content page");
  await logInTab(tab, "msg-1");

  info("Call watchResources with various configurations");

  // Watcher 1 only watches for CONSOLE_MESSAGE.
  // For this call site, unwatchResource will be called before onAvailable has
  // resolved.
  const messages1 = [];
  const onAvailable1 = createMessageCallback(messages1);
  const onWatcher1Ready = resourceWatcher.watchResources([CONSOLE_MESSAGE], {
    onAvailable: onAvailable1,
  });
  resourceWatcher.unwatchResources([CONSOLE_MESSAGE], {
    onAvailable: onAvailable1,
  });

  // Watcher 2 watches for CONSOLE_MESSAGE & another resource (ROOT_NODE).
  // Again unwatchResource will be called before onAvailable has resolved.
  // But unwatchResource is only called for CONSOLE_MESSAGE, not for ROOT_NODE.
  const messages2 = [];
  const onAvailable2 = createMessageCallback(messages2);
  const onWatcher2Ready = resourceWatcher.watchResources(
    [CONSOLE_MESSAGE, ROOT_NODE],
    {
      onAvailable: onAvailable2,
    }
  );
  resourceWatcher.unwatchResources([CONSOLE_MESSAGE], {
    onAvailable: onAvailable2,
  });

  // Watcher 3 watches for CONSOLE_MESSAGE, but we will not call unwatchResource
  // explicitly for it before the end of test. Used as a reference.
  const messages3 = [];
  const onAvailable3 = createMessageCallback(messages3);
  const onWatcher3Ready = resourceWatcher.watchResources([CONSOLE_MESSAGE], {
    onAvailable: onAvailable3,
  });

  info("Call unwatchResources for CONSOLE_MESSAGE on watcher 1 & 2");

  info("Wait for all watchers `watchResources` to resolve");
  await Promise.all([onWatcher1Ready, onWatcher2Ready, onWatcher3Ready]);
  ok(!hasMessage(messages1, "msg-1"), "Watcher 1 did not receive msg-1");
  ok(!hasMessage(messages2, "msg-1"), "Watcher 2 did not receive msg-1");
  ok(hasMessage(messages3, "msg-1"), "Watcher 3 received msg-1");

  info("Log a new message");
  await logInTab(tab, "msg-2");

  info("Wait until watcher 3 received the new message");
  await waitUntil(() => hasMessage(messages3, "msg-2"));

  ok(!hasMessage(messages1, "msg-2"), "Watcher 1 did not receive msg-2");
  ok(!hasMessage(messages2, "msg-2"), "Watcher 2 did not receive msg-2");

  targetCommand.destroy();
  await client.close();
});

function logInTab(tab, message) {
  return ContentTask.spawn(tab.linkedBrowser, message, function(_message) {
    content.console.log(_message);
  });
}

function hasMessage(messageResources, text) {
  return messageResources.find(
    resource => resource.message.arguments[0] === text
  );
}

// All resource watcher callbacks share the same pattern here: they add all
// console message resources to a provided `messages` array.
function createMessageCallback(messages) {
  const { CONSOLE_MESSAGE } = ResourceWatcher.TYPES;
  return async resources => {
    for (const resource of resources) {
      if (resource.resourceType === CONSOLE_MESSAGE) {
        messages.push(resource);
      }
    }
  };
}
