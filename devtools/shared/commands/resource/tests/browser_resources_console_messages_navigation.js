/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the resource command API around CONSOLE_MESSAGE when navigating
// tab and inner iframes to distinct origin/processes.

const TEST_URL = URL_ROOT_COM_SSL + "doc_console.html";
const TEST_IFRAME_URL = URL_ROOT_ORG_SSL + "doc_console_iframe.html";
const TEST_DOMAIN = "https://example.org";
add_task(async function () {
  const START_URL = "data:text/html;charset=utf-8,foo";
  const tab = await addTab(START_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  await testCrossProcessTabNavigation(tab.linkedBrowser, resourceCommand);
  await testCrossProcessIframeNavigation(tab.linkedBrowser, resourceCommand);

  targetCommand.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
});

async function testCrossProcessTabNavigation(browser, resourceCommand) {
  info(
    "Navigate the top level document from data: URI to a https document including remote iframes"
  );

  let doneResolve;
  const messages = [];
  const onConsoleLogsComplete = new Promise(resolve => (doneResolve = resolve));

  const onAvailable = resources => {
    messages.push(...resources);
    if (messages.length == 2) {
      doneResolve();
    }
  };

  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    { onAvailable }
  );

  const onLoaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, TEST_URL);
  await onLoaded;

  info("Wait for log message");
  await onConsoleLogsComplete;

  // messages are coming from different targets so the order isn't guaranteed
  const topLevelMessageResource = messages.find(resource =>
    resource.message.filename.startsWith(URL_ROOT_COM_SSL)
  );
  const iframeMessage = messages.find(resource =>
    resource.message.filename.startsWith("data:")
  );

  assertConsoleMessage(resourceCommand, topLevelMessageResource, {
    targetFront: resourceCommand.targetCommand.targetFront,
    messageText: "top-level document log",
  });
  assertConsoleMessage(resourceCommand, iframeMessage, {
    targetFront: isEveryFrameTargetEnabled
      ? resourceCommand.targetCommand
          .getAllTargets([resourceCommand.targetCommand.TYPES.FRAME])
          .find(t => t.url.startsWith("data:"))
      : resourceCommand.targetCommand.targetFront,
    messageText: "data url data log",
  });

  resourceCommand.unwatchResources([resourceCommand.TYPES.CONSOLE_MESSAGE], {
    onAvailable,
  });
}

async function testCrossProcessIframeNavigation(browser, resourceCommand) {
  info("Navigate an inner iframe from data: URI to a https remote URL");

  let doneResolve;
  const messages = [];
  const onConsoleLogsComplete = new Promise(resolve => (doneResolve = resolve));

  const onAvailable = resources => {
    messages.push(
      ...resources.filter(
        r => !r.message.arguments[0].startsWith("[WORKER] started")
      )
    );
    if (messages.length == 3) {
      doneResolve();
    }
  };

  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable,
    }
  );

  // messages are coming from different targets so the order isn't guaranteed
  const topLevelMessageResource = messages.find(resource =>
    resource.message.arguments[0].startsWith("top-level")
  );
  const dataUrlMessageResource = messages.find(resource =>
    resource.message.arguments[0].startsWith("data url")
  );

  // Assert cached messages from the previous top document
  assertConsoleMessage(resourceCommand, topLevelMessageResource, {
    messageText: "top-level document log",
  });
  assertConsoleMessage(resourceCommand, dataUrlMessageResource, {
    messageText: "data url data log",
  });

  // Navigate the iframe to another origin/process
  await SpecialPowers.spawn(browser, [TEST_IFRAME_URL], function (iframeUrl) {
    const iframe = content.document.querySelector("iframe");
    iframe.src = iframeUrl;
  });

  info("Wait for log message");
  await onConsoleLogsComplete;

  // iframeTarget will be different if Fission is on or off
  const iframeTarget = await getIframeTargetFront(
    resourceCommand.targetCommand
  );

  const iframeMessageResource = messages.find(resource =>
    resource.message.arguments[0].endsWith("iframe log")
  );
  assertConsoleMessage(resourceCommand, iframeMessageResource, {
    messageText: `${TEST_DOMAIN} iframe log`,
    targetFront: iframeTarget,
  });

  resourceCommand.unwatchResources([resourceCommand.TYPES.CONSOLE_MESSAGE], {
    onAvailable,
  });
}

function assertConsoleMessage(resourceCommand, messageResource, expected) {
  is(
    messageResource.resourceType,
    resourceCommand.TYPES.CONSOLE_MESSAGE,
    "Resource is a console message"
  );
  ok(messageResource.message, "message is wrapped into a message attribute");
  if (expected.targetFront) {
    is(
      messageResource.targetFront,
      expected.targetFront,
      "Message has the correct target front"
    );
  }
  is(
    messageResource.message.arguments[0],
    expected.messageText,
    "The correct type of message"
  );
}

async function getIframeTargetFront(targetCommand) {
  // If Fission/EFT is enabled, the iframe will have a dedicated target,
  // otherwise it will be debuggable via the top level target.
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    return targetCommand.targetFront;
  }
  const frameTargets = targetCommand.getAllTargets([targetCommand.TYPES.FRAME]);
  const browsingContextID = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return content.document.querySelector("iframe").browsingContext.id;
    }
  );
  const iframeTarget = frameTargets.find(target => {
    return target.browsingContextID == browsingContextID;
  });
  ok(iframeTarget, "Found the iframe target front");
  return iframeTarget;
}
