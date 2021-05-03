/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around CSS_MESSAGE
// Reproduces the CSS message assertions from devtools/shared/webconsole/test/chrome/test_page_errors.html

const { MESSAGE_CATEGORY } = require("devtools/shared/constants");

// Create a simple server so we have a nice sourceName in the resources packets.
const httpServer = createTestHTTPServer();
httpServer.registerPathHandler(`/test_css_messages.html`, (req, res) => {
  res.setStatusLine(req.httpVersion, 200, "OK");
  res.write(`<meta charset=utf8>
    <style>
      html {
        color: bloup;
      }
    </style>Test CSS Messages`);
});

const TEST_URI = `http://localhost:${httpServer.identity.primaryPort}/test_css_messages.html`;

add_task(async function() {
  await testWatchingCssMessages();
  await testWatchingCachedCssMessages();
});

async function testWatchingCssMessages() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  // Open a test tab
  const tab = await addTab(TEST_URI);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  const receivedMessages = [];
  const { onAvailable, onAllMessagesReceived } = setupOnAvailableFunction(
    targetCommand,
    receivedMessages,
    false
  );
  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_MESSAGE], {
    onAvailable,
  });

  info(
    "Now log CSS warning *after* the call to ResourceWatcher.watchResources and after " +
      "having received the existing message"
  );
  // We need to wait for the first CSS Warning as it is not a cached message; when we
  // start watching, the `cssErrorReportingEnabled` is checked on the target docShell, and
  // if it is false, we re-parse the stylesheets to get the messages.
  await BrowserTestUtils.waitForCondition(() => receivedMessages.length === 1);

  info("Trigger a CSS Warning");
  triggerCSSWarning(tab);

  info("Waiting for all expected CSS messages to be received");
  await onAllMessagesReceived;
  ok(true, "All the expected CSS messages were received");

  Services.console.reset();
  targetCommand.destroy();
  await client.close();
}

async function testWatchingCachedCssMessages() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  // Open a test tab
  const tab = await addTab(TEST_URI);

  // By default, the CSS Parser does not emit warnings at all, for performance matter.
  // Since we actually want the Parser to emit those messages _before_ we start listening
  // for CSS messages, we need to set the cssErrorReportingEnabled flag on the docShell.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.docShell.cssErrorReportingEnabled = true;
  });

  // Setting the docShell flag only indicates to the Parser that from now on, it should
  // emit warnings. But it does not automatically emit warnings for the existing CSS
  // errors in the stylesheets. So here we reload the tab, which will make the Parser
  // parse the stylesheets again, this time emitting warnings.
  const loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  // wait for the tab to be fully loaded
  await loaded;
  // and trigger more CSS warnings
  await triggerCSSWarning(tab);

  // At this point, all messages should be in the ConsoleService cache, and we can begin
  // to watch and check that we do retrieve those messages.
  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  const receivedMessages = [];
  const { onAvailable } = setupOnAvailableFunction(
    targetCommand,
    receivedMessages,
    true
  );
  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_MESSAGE], {
    onAvailable,
  });
  is(receivedMessages.length, 3, "Cached messages were retrieved as expected");

  Services.console.reset();
  targetCommand.destroy();
  await client.close();
}

function setupOnAvailableFunction(
  targetCommand,
  receivedMessages,
  isAlreadyExistingResource
) {
  // The expected messages are the CSS warnings:
  // - one for the rule in the style element
  // - two for the JS modified style we're doing in the test.
  const expectedMessages = [
    {
      pageError: {
        errorMessage: /Expected color but found ‘bloup’/,
        sourceName: /test_css_messages/,
        category: MESSAGE_CATEGORY.CSS_PARSER,
        timeStamp: /^\d+$/,
        error: false,
        warning: true,
      },
      cssSelectors: "html",
      isAlreadyExistingResource,
    },
    {
      pageError: {
        errorMessage: /Error in parsing value for ‘width’/,
        sourceName: /test_css_messages/,
        category: MESSAGE_CATEGORY.CSS_PARSER,
        timeStamp: /^\d+$/,
        error: false,
        warning: true,
      },
      isAlreadyExistingResource,
    },
    {
      pageError: {
        errorMessage: /Error in parsing value for ‘height’/,
        sourceName: /test_css_messages/,
        category: MESSAGE_CATEGORY.CSS_PARSER,
        timeStamp: /^\d+$/,
        error: false,
        warning: true,
      },
      isAlreadyExistingResource,
    },
  ];

  let done;
  const onAllMessagesReceived = new Promise(resolve => (done = resolve));
  const onAvailable = resources => {
    for (const resource of resources) {
      const { pageError } = resource;

      is(
        resource.targetFront,
        targetCommand.targetFront,
        "The targetFront property is the expected one"
      );

      if (!pageError.sourceName.includes("test_css_messages")) {
        info(`Ignore error from unknown source: "${pageError.sourceName}"`);
        continue;
      }

      const index = receivedMessages.length;
      receivedMessages.push(resource);

      info(
        `checking received css message #${index}: ${pageError.errorMessage}`
      );
      ok(pageError, "The resource has a pageError attribute");
      checkObject(resource, expectedMessages[index]);

      if (receivedMessages.length == expectedMessages.length) {
        done();
      }
    }
  };
  return { onAvailable, onAllMessagesReceived };
}

/**
 * Sets invalid values for width and height on the document's body style attribute.
 */
function triggerCSSWarning(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, function frameScript() {
    content.document.body.style.width = "red";
    content.document.body.style.height = "blue";
  });
}
