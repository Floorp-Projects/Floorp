/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around THREAD_STATE

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

const BREAKPOINT_TEST_URL = URL_ROOT_SSL + "breakpoint_document.html";
const REMOTE_IFRAME_URL =
  "https://example.org/document-builder.sjs?html=" +
  encodeURIComponent("<script>debugger;</script>");

add_task(async function () {
  // Check hitting the "debugger;" statement before and after calling
  // watchResource(THREAD_TYPES). Both should break. First will
  // be a cached resource and second will be a live one.
  await checkBreakpointBeforeWatchResources();
  await checkBreakpointAfterWatchResources();

  // Check setting a real breakpoint on a given line
  await checkRealBreakpoint();

  // Check the "pause on exception" setting
  await checkPauseOnException();

  // Check an edge case where spamming setBreakpoints calls causes issues
  await checkSetBeforeWatch();

  // Check debugger statement for (remote) iframes
  await checkDebuggerStatementInIframes();

  // Check the behavior of THREAD_STATE against a multiprocess usecase
  await testMultiprocessThreadState();
});

async function checkBreakpointBeforeWatchResources() {
  info(
    "Check whether ResourceCommand gets existing breakpoint, being hit before calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  // Ensure that the target front is initialized early from TargetCommand.onTargetAvailable
  // By the time `initResourceCommand` resolves, it should already be initialized.
  info(
    "Verify that TargetFront's initialized is resolved after having calling attachAndInitThread"
  );
  await targetCommand.targetFront.initialized;

  // We have to ensure passing any thread configuration in order to have breakpoints being handled.
  await commands.threadConfigurationCommand.updateConfiguration({
    skipBreakpoints: false,
  });

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    1,
    "Got the THREAD_STATE's related to the debugger statement"
  );
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "debuggerStatement",
    },
    frame: {
      type: "call",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "runDebuggerStatement",
      // arguments: []
      where: {
        line: 17,
        column: 6,
      },
    },
  });

  const { threadFront } = targetCommand.targetFront;
  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function checkBreakpointAfterWatchResources() {
  info(
    "Check whether ResourceCommand gets breakpoint hit after calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  await waitFor(
    () => availableResources.length == 1,
    "Got the THREAD_STATE related to the debugger statement"
  );
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "debuggerStatement",
    },
    frame: {
      type: "call",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "runDebuggerStatement",
      // arguments: []
      where: {
        line: 17,
        column: 6,
      },
    },
  });

  // treadFront is created and attached while calling watchResources
  const { threadFront } = targetCommand.targetFront;

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function checkRealBreakpoint() {
  info(
    "Check whether ResourceCommand gets breakpoint set via the thread Front (instead of just debugger statements)"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  // treadFront is created and attached while calling watchResources
  const { threadFront } = targetCommand.targetFront;

  // We have to call `sources` request, otherwise the Thread Actor
  // doesn't start watching for sources, and ignore the setBreakpoint call
  // as it doesn't have any source registered.
  await threadFront.getSources();

  await threadFront.setBreakpoint(
    { sourceUrl: BREAKPOINT_TEST_URL, line: 14 },
    {}
  );

  info("Run the test function where we set a breakpoint");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.testFunction();
  });

  await waitFor(
    () => availableResources.length == 1,
    "Got the THREAD_STATE related to the debugger statement"
  );
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "breakpoint",
    },
    frame: {
      type: "call",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "testFunction",
      // arguments: []
      where: {
        line: 14,
        column: 6,
      },
    },
  });

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function checkPauseOnException() {
  info(
    "Check whether ResourceCommand gets breakpoint for exception (when explicitly requested)"
  );

  const tab = await addTab(
    "data:text/html,<meta charset=utf8><script>a.b.c.d</script>"
  );

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  await commands.threadConfigurationCommand.updateConfiguration({
    pauseOnExceptions: true,
  });

  info("Reload the page, in order to trigger exception on load");
  const reloaded = reloadBrowser();

  await waitFor(
    () => availableResources.length == 1,
    "Got the THREAD_STATE related to the debugger statement"
  );
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "exception",
    },
    frame: {
      type: "global",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "(global)",
      // arguments: []
      where: {
        line: 1,
        column: 27,
      },
    },
  });

  const { threadFront } = targetCommand.targetFront;
  await threadFront.resume();
  info("Wait for page to finish reloading after resume");
  await reloaded;

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function checkSetBeforeWatch() {
  info(
    "Verify bug 1683139 - D103068, where setting a breakpoint before watching for thread state, avoid receiving the paused state"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  // Instantiate the thread front in order to be able to set a breakpoint before watching for thread state
  info("Attach the top level thread actor");
  await targetCommand.targetFront.attachAndInitThread(targetCommand);
  const { threadFront } = targetCommand.targetFront;

  // We have to call `sources` request, otherwise the Thread Actor
  // doesn't start watching for sources, and ignore the setBreakpoint call
  // as it doesn't have any source registered.
  await threadFront.getSources();

  // Set the breakpoint before trying to hit it
  await threadFront.setBreakpoint(
    { sourceUrl: BREAKPOINT_TEST_URL, line: 14, column: 6 },
    {}
  );

  info("Run the test function where we set a breakpoint");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.testFunction();
  });

  // bug 1683139 - D103068. Re-setting the breakpoint just before watching for thread state
  // prevented to receive the paused state change.
  await threadFront.setBreakpoint(
    { sourceUrl: BREAKPOINT_TEST_URL, line: 14, column: 6 },
    {}
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  await waitFor(() => {
    return availableResources.length == 1;
  }, "Got the THREAD_STATE related to the debugger statement");
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "breakpoint",
    },
    frame: {
      type: "call",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "testFunction",
      // arguments: []
      where: {
        line: 14,
        column: 6,
      },
    },
  });

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function checkDebuggerStatementInIframes() {
  info("Check whether ResourceCommand gets breakpoint for (remote) iframes");

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { commands, resourceCommand, targetCommand } =
    await initResourceCommand(tab);

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  info("Inject the iframe with an inline 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REMOTE_IFRAME_URL],
    async function (url) {
      const iframe = content.document.createElement("iframe");
      iframe.src = url;
      content.document.body.appendChild(iframe);
    }
  );

  await waitFor(
    () => availableResources.length == 1,
    "Got the THREAD_STATE related to the iframe's debugger statement"
  );
  const threadState = availableResources.pop();

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "debuggerStatement",
    },
    frame: {
      type: "global",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "(global)",
      // arguments: []
      where: {
        line: 1,
        column: 8,
      },
    },
  });

  const iframeTarget = threadState.targetFront;
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    is(
      iframeTarget.url,
      REMOTE_IFRAME_URL,
      "With fission/EFT, the pause is from the iframe's target"
    );
  } else {
    is(
      iframeTarget,
      targetCommand.targetFront,
      "Without fission/EFT, the pause is from the top level target"
    );
  }
  const { threadFront } = iframeTarget;

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetCommand.destroy();
  await commands.destroy();
}

async function testMultiprocessThreadState() {
  // Ensure debugging the content processes and the tab
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const { commands, resourceCommand, targetCommand } =
    await initMultiProcessResourceCommand();

  info("Call watchResources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable() {},
  });
  await resourceCommand.watchResources([resourceCommand.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  const onResumed = ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  await waitFor(
    () => availableResources.length == 1,
    "Got the THREAD_STATE related to the debugger statement"
  );
  const threadState = availableResources.pop();
  ok(threadState.targetFront.targetType, "process");
  ok(threadState.targetFront.threadFront.state, "paused");

  assertPausedResource(threadState, {
    state: "paused",
    why: {
      type: "debuggerStatement",
    },
    frame: {
      type: "call",
      asyncCause: null,
      state: "on-stack",
      // this: object actor's form referring to `this` variable
      displayName: "runDebuggerStatement",
      // arguments: []
      where: {
        line: 17,
        column: 6,
      },
    },
  });

  await threadState.targetFront.threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  // This is an important check, which verify that no unexpected pause happens.
  // We might spawn a Thread Actor for the WindowGlobal target, which might pause the thread a second time,
  // whereas we only expect the ContentProcess target actor to pause on all JS files of the related content process.
  info("Wait for the content page thread to resume its execution");
  await onResumed;
  is(availableResources.length, 0, "There should be no other pause");

  targetCommand.destroy();
  await commands.destroy();
}

async function assertPausedResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.THREAD_STATE,
    "Resource type is correct"
  );
  is(resource.state, "paused", "state attribute is correct");
  is(resource.why.type, expected.why.type, "why.type attribute is correct");
  is(
    resource.frame.type,
    expected.frame.type,
    "frame.type attribute is correct"
  );
  is(
    resource.frame.asyncCause,
    expected.frame.asyncCause,
    "frame.asyncCause attribute is correct"
  );
  is(
    resource.frame.state,
    expected.frame.state,
    "frame.state attribute is correct"
  );
  is(
    resource.frame.displayName,
    expected.frame.displayName,
    "frame.displayName attribute is correct"
  );
  is(
    resource.frame.where.line,
    expected.frame.where.line,
    "frame.where.line attribute is correct"
  );
  is(
    resource.frame.where.column,
    expected.frame.where.column,
    "frame.where.column attribute is correct"
  );
}

async function assertResumedResource(resource) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.THREAD_STATE,
    "Resource type is correct"
  );
  is(resource.state, "resumed", "state attribute is correct");
}
