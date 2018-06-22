/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = TEST_URI_ROOT + "contextual_identity.html";

// Opens `uri' in a new tab with the provided userContextId.
// Returns the newly opened tab and browser.
async function addTabInUserContext(uri, userContextId) {
  const tab = await addTab(uri, { userContextId });
  const browser = tab.linkedBrowser;
  return { tab, browser };
}

async function sendMessages(receiver) {
  const channelName = "contextualidentity-broadcastchannel";

  // reflect the received message on title
  await ContentTask.spawn(receiver.browser, channelName, function(name) {
    content.testPromise = new content.Promise(resolve => {
      content.bc = new content.BroadcastChannel(name);
      content.bc.onmessage = function(e) {
        content.document.title += e.data;
        resolve();
      };
    });
  });

  const sender1 = await addTabInUserContext(TEST_URL, 1);
  const sender2 = await addTabInUserContext(TEST_URL, 2);
  sender1.message = "Message from user context #1";
  sender2.message = "Message from user context #2";

  // send a message from a tab in different user context first
  // then send a message from a tab in the same user context
  for (const sender of [sender1, sender2]) {
    await ContentTask.spawn(
      sender.browser,
      { name: channelName, message: sender.message },
      function(opts) {
        const bc = new content.BroadcastChannel(opts.name);
        bc.postMessage(opts.message);
      });
  }

  return {
    sender1,
    sender2,
    receiver,
  };
}

async function verifyResults({ sender1, sender2, receiver }) {
  // Since sender1 sends before sender2, if the title is exactly
  // sender2's message, sender1's message must've been blocked
  await ContentTask.spawn(receiver.browser, sender2.message,
    async function(message) {
      await content.testPromise.then(function() {
        is(content.document.title, message,
           "should only receive messages from the same user context");
      });
    }
  );

  gBrowser.removeTab(sender1.tab);
  gBrowser.removeTab(sender2.tab);
}

add_task(async function() {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["privacy.userContext.enabled", true]
    ]
  });
});

add_task(async function() {
  info("Checking broadcast channel, send outside RDM, verify inside RDM");
  const receiver = await addTabInUserContext(TEST_URL, 2);
  const tabs = await sendMessages(receiver);
  const { ui } = await openRDM(receiver.tab);
  receiver.browser = ui.getViewportBrowser();
  await verifyResults(tabs);
  await removeTab(receiver.tab);
});

add_task(async function() {
  info("Checking broadcast channel, send inside RDM, verify inside RDM");
  const receiver = await addTabInUserContext(TEST_URL, 2);
  const { ui } = await openRDM(receiver.tab);
  receiver.browser = ui.getViewportBrowser();
  const tabs = await sendMessages(receiver);
  await verifyResults(tabs);
  await removeTab(receiver.tab);
});
