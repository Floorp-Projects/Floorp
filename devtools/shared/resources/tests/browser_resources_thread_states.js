/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around THREAD_STATE

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const BREAKPOINT_TEST_URL = URL_ROOT_SSL + "breakpoint_document.html";
const REMOTE_IFRAME_URL =
  "https://example.org/document-builder.sjs?html=" +
  encodeURIComponent("<script>debugger;</script>");

add_task(async function() {
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
});

async function checkBreakpointBeforeWatchResources() {
  info(
    "Check whether ResourceWatcher gets existing breakpoint, being hit before calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  // Attach the thread actor before running the debugger statement,
  // so that it is correctly catched by the thread actor.
  info("Attach the top level target");
  await targetCommand.targetFront.attach();
  // Init the Thread actor via attachAndInitThread in order to ensure
  // memoizing the thread front and avoid attaching it twice
  info("Attach the top level thread actor");
  await targetCommand.targetFront.attachAndInitThread(targetCommand);

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
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
  await client.close();
}

async function checkBreakpointAfterWatchResources() {
  info(
    "Check whether ResourceWatcher gets breakpoint hit after calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
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
  await client.close();
}

async function checkRealBreakpoint() {
  info(
    "Check whether ResourceWatcher gets breakpoint set via the thread Front (instead of just debugger statements)"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
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
  await client.close();
}

async function checkPauseOnException() {
  info(
    "Check whether ResourceWatcher gets breakpoint for exception (when explicitly requested)"
  );

  const tab = await addTab(
    "data:text/html,<meta charset=utf8><script>a.b.c.d</script>"
  );

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no THREAD_STATE when calling watchResources"
  );

  // treadFront is created and attached while calling watchResources
  const { threadFront } = targetCommand.targetFront;
  await threadFront.reconfigure({ pauseOnExceptions: true });

  info("Reload the page, in order to trigger exception on load");
  const reloaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();

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
        column: 0,
      },
    },
  });

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
  await client.close();
}

async function checkSetBeforeWatch() {
  info(
    "Verify bug 1683139 - D103068, where setting a breakpoint before watching for thread state, avoid receiving the paused state"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  // Attach the target in order to create the thread actor
  info("Attach the top level target");
  await targetCommand.targetFront.attach();
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
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
    onAvailable: resources => availableResources.push(...resources),
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
  await client.close();
}

async function checkDebuggerStatementInIframes() {
  info("Check whether ResourceWatcher gets breakpoint for (remote) iframes");

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.THREAD_STATE], {
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
    async function(url) {
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
        column: 0,
      },
    },
  });

  const iframeTarget = threadState.targetFront;
  if (isFissionEnabled()) {
    is(
      iframeTarget.url,
      REMOTE_IFRAME_URL,
      "With fission, the pause is from the iframe's target"
    );
  } else {
    is(
      iframeTarget,
      targetCommand.targetFront,
      "Without fission, the pause is from the top level target"
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
  await client.close();
}

async function assertPausedResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceWatcher.TYPES.THREAD_STATE,
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
    ResourceWatcher.TYPES.THREAD_STATE,
    "Resource type is correct"
  );
  is(resource.state, "resumed", "state attribute is correct");
}
