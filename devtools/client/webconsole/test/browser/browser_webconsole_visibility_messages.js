/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check messages logged when console not visible are displayed when
// the user show the console again.

const HTML = `
  <!DOCTYPE html>
  <html>
    <body>
      <h1>Test console visibility update</h1>
      <script>
        function log(str) {
          console.log(str);
        }
      </script>
    </body>
  </html>
`;
const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);
const MESSAGES_COUNT = 10;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  info("Log one message in the console");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.log("in-console log");
  });
  await waitFor(() => findMessage(hud, "in-console log"));

  info("select the inspector");
  await toolbox.selectTool("inspector");

  info("Wait for console to be hidden");
  const { document } = hud.iframeWindow;
  await waitFor(() => document.visibilityState == "hidden");

  const onAllMessagesInStore = new Promise(done => {
    const store = hud.ui.wrapper.getStore();
    store.subscribe(() => {
      const messages = store.getState().messages.messagesById.size;
      // Also consider the "in-console log" message
      if (messages == MESSAGES_COUNT + 1) {
        done();
      }
    });
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, [MESSAGES_COUNT], count => {
    for (let i = 1; i <= count; i++) {
      content.wrappedJSObject.log("in-inspector log " + i);
    }
  });

  info("Waiting for all messages to be logged into the store");
  await onAllMessagesInStore;

  const count = await findMessages(hud, "in-inspector");
  is(count, 0, "No messages from the inspector actually appear in the console");

  info("select back the console");
  await toolbox.selectTool("webconsole");

  info("And wait for all messages to be visible");
  const waitForMessagePromises = [];
  for (let j = 1; j <= MESSAGES_COUNT; j++) {
    waitForMessagePromises.push(
      waitFor(() => findMessage(hud, "in-inspector log " + j))
    );
  }

  await Promise.all(waitForMessagePromises);
  ok(
    true,
    "All the messages logged when the console was hidden were displayed."
  );
});

// Similar scenario, but with the split console on the inspector panel.
// Here, the messages should still be logged.
add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  info("Log one message in the console");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.log("in-console log");
  });
  await waitFor(() => findMessage(hud, "in-console log"));

  info("select the inspector");
  await toolbox.selectTool("inspector");

  info("Wait for console to be hidden");
  const { document } = hud.iframeWindow;
  await waitFor(() => document.visibilityState == "hidden");

  await toolbox.openSplitConsole();

  await ContentTask.spawn(gBrowser.selectedBrowser, [MESSAGES_COUNT], count => {
    for (let i = 1; i <= count; i++) {
      content.wrappedJSObject.log("in-inspector log " + i);
    }
  });

  info("Wait for all messages to be visible in the split console");
  const waitForMessagePromises = [];
  for (let j = 1; j <= MESSAGES_COUNT; j++) {
    waitForMessagePromises.push(
      waitFor(() => findMessage(hud, "in-inspector log " + j))
    );
  }

  await Promise.all(waitForMessagePromises);
  ok(true, "All the messages logged when we are using the split console");

  await toolbox.closeSplitConsole();
});
