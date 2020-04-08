/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to see if the cached messages are displayed when the console UI is opened.

"use strict";

// See Bug 1570524.
requestLongerTimeout(2);

const TEST_URI = `data:text/html,<meta charset=utf8><h1>Test cached messages</h1>
  <style>
    h1 {
      color: cssColorBug611032;
    }
  </style>
  <script>
    function logException() {
      return new Promise(resolve => {
        setTimeout(() => {
          let foo = {};
          resolve();
          foo.unknown();
        }, 0);
      })
    }
  </script>`;

add_task(async function() {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }
  // Enable CSS and XHR filters for the test.
  await pushPref("devtools.webconsole.filter.css", true);
  await pushPref("devtools.webconsole.filter.netxhr", true);

  await addTab(TEST_URI);

  info("Log different type of messages to fill the cache");
  await logMessages();

  info("Open the console");
  let hud = await openConsole();

  // We only start watching network requests when opening the toolbox.
  await testMessagesVisibility(hud, false);

  info("Close the toolbox and reload the tab");
  await closeToolbox();
  await reloadPage();

  info(
    "Open the toolbox with the inspector selected, so we can get network messages"
  );
  await openInspector();

  info("Log different type of messages to fill the cache");
  await logMessages();

  info("Open the console");
  hud = await openConsole();

  await testMessagesVisibility(hud);

  info("Close the toolbox");
  await closeToolbox();

  info("Open the console again");
  hud = await openConsole();
  // The network messages don't persist.
  await testMessagesVisibility(hud, false);
});

async function logMessages() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    const wait = () =>
      new Promise(res => content.wrappedJSObject.setTimeout(res, 100));

    content.wrappedJSObject.console.log("log Bazzle");
    await wait();

    await content.wrappedJSObject.logException();
    await wait();

    await content.wrappedJSObject.fetch(
      "http://mochi.test:8888/browser/devtools/client/webconsole/test/browser/sjs_cors-test-server.sjs?1",
      { mode: "cors" }
    );
    await wait();

    content.wrappedJSObject.console.error("error Bazzle");
    await wait();

    await content.wrappedJSObject.logException();
    await wait();

    await content.wrappedJSObject.fetch(
      "http://mochi.test:8888/browser/devtools/client/webconsole/test/browser/sjs_cors-test-server.sjs?2"
    );

    content.wrappedJSObject.console.info("info Bazzle");
    await wait();
  });
}

async function testMessagesVisibility(hud, checkNetworkMessage = true) {
  // wait for the last logged message to be displayed
  await waitFor(() => findMessage(hud, "info Bazzle", ".message.info"));

  const messages = Array.from(hud.ui.outputNode.querySelectorAll(".message"));
  const EXPECTED_MESSAGES = [
    {
      text: "log Bazzle",
      category: "log",
    },
    {
      text: "foo.unknown is not a function",
      category: "error",
    },
    {
      text: "sjs_cors-test-server.sjs?1",
      category: "network",
    },
    {
      text: "error Bazzle",
      category: "error",
    },
    {
      text: "foo.unknown is not a function",
      category: "error",
    },
    {
      text: "sjs_cors-test-server.sjs?2",
      category: "network",
    },
    {
      text: "info Bazzle",
      category: "info",
    },
  ].filter(({ category }) => checkNetworkMessage || category != "network");

  // Clone the original array so we can use it later
  const expectedMessages = [...EXPECTED_MESSAGES];
  for (const message of messages) {
    const [expectedMessage] = expectedMessages;
    if (
      message.classList.contains(expectedMessage.category) &&
      message.textContent.includes(expectedMessage.text)
    ) {
      ok(
        true,
        `The ${expectedMessage.category} message "${expectedMessage.text}" is visible at the expected place`
      );
      expectedMessages.shift();
      if (expectedMessages.length === 0) {
        ok(
          true,
          "All the expected messages were found at the expected position"
        );
        break;
      }
    }
  }

  if (expectedMessages.length > 0) {
    ok(
      false,
      `Some messages are not visible or not in the expected order. Expected to find: \n\n${EXPECTED_MESSAGES.map(
        ({ text }) => text
      ).join("\n")}\n\nGot: \n\n${messages
        .map(message => `${message.querySelector(".message-body").textContent}`)
        .join("\n")}`
    );
  }

  // We can't assert the CSS warning position, so we only check that it's visible.
  await waitFor(() =>
    findMessage(hud, "cssColorBug611032", ".message.warn.css")
  );
  ok(true, "css warning message is visible");
}
