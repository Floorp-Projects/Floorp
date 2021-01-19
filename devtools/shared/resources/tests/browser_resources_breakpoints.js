/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around BREAKPOINT

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const BREAKPOINT_TEST_URL = URL_ROOT_SSL + "breakpoint_document.html";

add_task(async function() {
  await checkBreakpointBeforeWatchResources();

  await checkBreakpointAfterWatchResources();

  await checkRealBreakpoint();

  await checkPauseOnException();
});

async function checkBreakpointBeforeWatchResources() {
  info(
    "Check whether ResourceWatcher gets existing breakpoint, being hit before calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  // Attach the thread actor before running the debugger statement,
  // so that it is correctly catched by the thread actor.
  info("Attach the top level target");
  await targetList.targetFront.attach();
  info("Attach the top level thread actor");
  const threadFront = await targetList.targetFront.attachThread();

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    1,
    "Got the breakpoint related to the debugger statement"
  );
  const breakpoint = availableResources.pop();

  assertPausedResource(breakpoint, {
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

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetList.destroy();
  await client.close();
}

async function checkBreakpointAfterWatchResources() {
  info(
    "Check whether ResourceWatcher gets breakpoint hit after calling watchResources"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no breakpoint when calling watchResources"
  );

  info("Run the 'debugger' statement");
  // Note that we do not wait for the resolution of spawn as it will be paused
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.window.wrappedJSObject.runDebuggerStatement();
  });

  await waitFor(
    () => availableResources.length == 1,
    "Got the breakpoint related to the debugger statement"
  );
  const breakpoint = availableResources.pop();

  assertPausedResource(breakpoint, {
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
  const { threadFront } = targetList.targetFront;

  await threadFront.resume();

  await waitFor(
    () => availableResources.length == 1,
    "Wait until we receive the resumed event"
  );

  const resumed = availableResources.pop();

  assertResumedResource(resumed);

  targetList.destroy();
  await client.close();
}

async function checkRealBreakpoint() {
  info(
    "Check whether ResourceWatcher gets breakpoint set via the thread Front (instead of just debugger statements)"
  );

  const tab = await addTab(BREAKPOINT_TEST_URL);

  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no breakpoint when calling watchResources"
  );

  // treadFront is created and attached while calling watchResources
  const { threadFront } = targetList.targetFront;

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
    "Got the breakpoint related to the debugger statement"
  );
  const breakpoint = availableResources.pop();

  assertPausedResource(breakpoint, {
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

  targetList.destroy();
  await client.close();
}

async function checkPauseOnException() {
  info(
    "Check whether ResourceWatcher gets breakpoint for exception (when explicitly requested)"
  );

  const tab = await addTab(
    "data:text/html,<meta charset=utf8><script>a.b.c.d</script>"
  );

  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  info("Call watchResources");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: resources => availableResources.push(...resources),
  });

  is(
    availableResources.length,
    0,
    "Got no breakpoint when calling watchResources"
  );

  // treadFront is created and attached while calling watchResources
  const { threadFront } = targetList.targetFront;
  await threadFront.reconfigure({ pauseOnExceptions: true });

  info("Reload the page, in order to trigger exception on load");
  const reloaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();

  await waitFor(
    () => availableResources.length == 1,
    "Got the breakpoint related to the debugger statement"
  );
  const breakpoint = availableResources.pop();

  assertPausedResource(breakpoint, {
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

  targetList.destroy();
  await client.close();
}

async function assertPausedResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceWatcher.TYPES.BREAKPOINT,
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
    ResourceWatcher.TYPES.BREAKPOINT,
    "Resource type is correct"
  );
  is(resource.state, "resumed", "state attribute is correct");
}
