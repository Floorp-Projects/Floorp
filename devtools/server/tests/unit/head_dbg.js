/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: ["error", {"vars": "local"}] */
/* eslint-disable no-shadow */

"use strict";
var CC = Components.Constructor;

// Populate AppInfo before anything (like the shared loader) accesses
// System.appinfo, which is a lazy getter.
const appInfo = ChromeUtils.import("resource://testing-common/AppInfo.jsm");
appInfo.updateAppInfo({
  ID: "devtools@tests.mozilla.org",
  name: "devtools-tests",
  version: "1",
  platformVersion: "42",
  crashReporter: true,
});

const { require, loader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);
const { worker } = ChromeUtils.import(
  "resource://devtools/shared/worker/loader.js"
);
const defer = require("devtools/shared/defer");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");

const Services = require("Services");
// Always log packets when running tests. runxpcshelltests.py will throw
// the output away anyway, unless you give it the --verbose flag.
Services.prefs.setBoolPref("devtools.debugger.log", false);
// Enable remote debugging for the relevant tests.
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");
const { DebuggerServer } = require("devtools/server/debugger-server");
const { DebuggerServer: WorkerDebuggerServer } = worker.require(
  "devtools/server/debugger-server"
);
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const ObjectClient = require("devtools/shared/client/object-client");
const { LongStringFront } = require("devtools/shared/fronts/string");
const { TargetFactory } = require("devtools/client/framework/target");

const { addDebuggerToGlobal } = ChromeUtils.import(
  "resource://gre/modules/jsdebugger.jsm"
);

const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
  Ci.nsIPrincipal
);

var { loadSubScript, loadSubScriptWithOptions } = Services.scriptloader;

/**
 * Initializes any test that needs to work with add-ons.
 */
function startupAddonsManager() {
  // Create a directory for extensions.
  const profileDir = do_get_profile().clone();
  profileDir.append("extensions");

  const internalManager = Cc["@mozilla.org/addons/integration;1"]
    .getService(Ci.nsIObserver)
    .QueryInterface(Ci.nsITimerCallback);

  internalManager.observe(null, "addons-startup", null);
}

async function createTargetForFakeTab(title) {
  const client = await startTestDebuggerServer(title);

  const tabs = await listTabs(client);
  return findTab(tabs, title);
}

async function createTargetForMainProcess() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();

  return client.mainRoot.getMainProcess();
}

/**
 * Create a MemoryFront for a fake test tab.
 */
async function createTabMemoryFront() {
  const target = await createTargetForFakeTab("test_memory");

  // MemoryFront requires the HeadSnapshotActor actor to be available
  // as a global actor. This isn't registered by startTestDebuggerServer which
  // only register the target actors and not the browser ones.
  DebuggerServer.registerActors({ browser: true });

  const memoryFront = await target.getFront("memory");
  await memoryFront.attach();

  registerCleanupFunction(async () => {
    await memoryFront.detach();

    // On XPCShell, the target isn't for a local tab and so target.destroy
    // won't close the client. So do it so here. It will automatically destroy the target.
    await target.client.close();
  });

  return { target, memoryFront };
}

/**
 * Create a PromisesFront for a fake test tab.
 */
async function createTabPromisesFront() {
  const title = "test_promises";
  const target = await createTargetForFakeTab(title);

  // Retrieve the debuggee create by createTargetForFakeTab
  const debuggee = DebuggerServer.getTestGlobal(title);

  const promisesFront = await target.getFront("promises");

  registerCleanupFunction(async () => {
    // On XPCShell, the target isn't for a local tab and so target.destroy
    // won't close the client. So do it so here. It will automatically destroy the target.
    await target.client.close();
  });

  return { debuggee, client: target.client, promisesFront };
}

/**
 * Create a PromisesFront for the main process target actor.
 */
async function createMainProcessPromisesFront() {
  const target = await createTargetForMainProcess();

  const promisesFront = await target.getFront("promises");

  registerCleanupFunction(async () => {
    // For XPCShell, the main process target actor is ContentProcessTargetActor
    // which doesn't expose any `detach` method. So that the target actor isn't
    // destroyed when calling target.destroy.
    // Close the client to cleanup everything.
    await target.client.close();
  });

  return { client: target.client, promisesFront };
}

/**
 * Same as createTabMemoryFront but attaches the MemoryFront to the MemoryActor
 * scoped to the full runtime rather than to a tab.
 */
async function createMainProcessMemoryFront() {
  const target = await createTargetForMainProcess();

  const memoryFront = await target.getFront("memory");
  await memoryFront.attach();

  registerCleanupFunction(async () => {
    await memoryFront.detach();
    // For XPCShell, the main process target actor is ContentProcessTargetActor
    // which doesn't expose any `detach` method. So that the target actor isn't
    // destroyed when calling target.destroy.
    // Close the client to cleanup everything.
    await target.client.close();
  });

  return { client: target.client, memoryFront };
}

function createLongStringFront(conn, form) {
  // CAUTION -- do not replicate in the codebase. Instead, use marshalling
  // This code is simulating how the LongStringFront would be created by protocol.js
  // We should not use it like this in the codebase, this is done only for testing
  // purposes until we can return a proper LongStringFront from the server.
  const front = new LongStringFront(conn, form);
  front.actorID = form.actor;
  front.manage(front);
  return front;
}

function createTestGlobal(name) {
  const sandbox = Cu.Sandbox(
    Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
  );
  sandbox.__name = name;
  return sandbox;
}

function connect(client) {
  dump("Connecting client.\n");
  return client.connect();
}

function close(client) {
  dump("Closing client.\n");
  return client.close();
}

function listTabs(client) {
  dump("Listing tabs.\n");
  return client.mainRoot.listTabs();
}

function findTab(tabs, title) {
  dump("Finding tab with title '" + title + "'.\n");
  for (const tab of tabs) {
    if (tab.title === title) {
      return tab;
    }
  }
  return null;
}

function waitForNewSource(threadFront, url) {
  dump("Waiting for new source with url '" + url + "'.\n");
  return waitForEvent(threadFront, "newSource", function(packet) {
    return packet.source.url === url;
  });
}

function attachThread(targetFront, options = {}) {
  dump("Attaching to thread.\n");
  return targetFront.attachThread(options);
}

function resume(threadFront) {
  dump("Resuming thread.\n");
  return threadFront.resume();
}

function getSources(threadFront) {
  dump("Getting sources.\n");
  return threadFront.getSources();
}

function findSource(sources, url) {
  dump("Finding source with url '" + url + "'.\n");
  for (const source of sources) {
    if (source.url === url) {
      return source;
    }
  }
  return null;
}

function waitForPause(threadFront) {
  dump("Waiting for pause.\n");
  return waitForEvent(threadFront, "paused");
}

function waitForProperty(dbg, property) {
  return new Promise(resolve => {
    Object.defineProperty(dbg, property, {
      set(newValue) {
        resolve(newValue);
      },
    });
  });
}

function setBreakpoint(threadFront, location) {
  dump("Setting breakpoint.\n");
  return threadFront.setBreakpoint(location, {});
}

function getPrototypeAndProperties(objClient) {
  dump("getting prototype and properties.\n");

  return new Promise(resolve => {
    objClient.getPrototypeAndProperties(response => resolve(response));
  });
}

function dumpn(msg) {
  dump("DBG-TEST: " + msg + "\n");
}

function testExceptionHook(ex) {
  try {
    do_report_unexpected_exception(ex);
  } catch (e) {
    return { throw: e };
  }
  return undefined;
}

// Convert an nsIScriptError 'flags' value into an appropriate string.
function scriptErrorFlagsToKind(flags) {
  let kind;
  if (flags & Ci.nsIScriptError.warningFlag) {
    kind = "warning";
  }
  if (flags & Ci.nsIScriptError.exceptionFlag) {
    kind = "exception";
  } else {
    kind = "error";
  }

  if (flags & Ci.nsIScriptError.strictFlag) {
    kind = "strict " + kind;
  }

  return kind;
}

// Register a console listener, so console messages don't just disappear
// into the ether.
var errorCount = 0;
var listener = {
  observe: function(message) {
    try {
      let string;
      errorCount++;
      try {
        // If we've been given an nsIScriptError, then we can print out
        // something nicely formatted, for tools like Emacs to pick up.
        message.QueryInterface(Ci.nsIScriptError);
        dumpn(
          message.sourceName +
            ":" +
            message.lineNumber +
            ": " +
            scriptErrorFlagsToKind(message.flags) +
            ": " +
            message.errorMessage
        );
        string = message.errorMessage;
      } catch (e1) {
        // Be a little paranoid with message, as the whole goal here is to lose
        // no information.
        try {
          string = "" + message.message;
        } catch (e2) {
          string = "<error converting error message to string>";
        }
      }

      // Make sure we exit all nested event loops so that the test can finish.
      while (
        DebuggerServer &&
        DebuggerServer.xpcInspector &&
        DebuggerServer.xpcInspector.eventLoopNestLevel > 0
      ) {
        DebuggerServer.xpcInspector.exitNestedEventLoop();
      }

      // In the world before bug 997440, exceptions were getting lost because of
      // the arbitrary JSContext being used in nsXPCWrappedJS::CallMethod.
      // In the new world, the wanderers have returned. However, because of the,
      // currently very-broken, exception reporting machinery in
      // nsXPCWrappedJS these get reported as errors to the console, even if
      // there's actually JS on the stack above that will catch them.  If we
      // throw an error here because of them our tests start failing.  So, we'll
      // just dump the message to the logs instead, to make sure the information
      // isn't lost.
      dumpn("head_dbg.js observed a console message: " + string);
    } catch (_) {
      // Swallow everything to avoid console reentrancy errors. We did our best
      // to log above, but apparently that didn't cut it.
    }
  },
};

Services.console.registerListener(listener);

function testGlobal(name) {
  const sandbox = Cu.Sandbox(
    Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
  );
  sandbox.__name = name;
  return sandbox;
}

function addTestGlobal(name, server = DebuggerServer) {
  const global = testGlobal(name);
  server.addTestGlobal(global);
  return global;
}

// List the DebuggerClient |client|'s tabs, look for one whose title is
// |title|, and apply |callback| to the packet's entry for that tab.
async function getTestTab(client, title) {
  const tabs = await client.mainRoot.listTabs();
  for (const tab of tabs) {
    if (tab.title === title) {
      return tab;
    }
  }
  return null;
}

// Attach to |client|'s tab whose title is |title|; and return the targetFront instance
// referring to that tab.
async function attachTestTab(client, title) {
  const targetFront = await getTestTab(client, title);
  await targetFront.attach();
  return targetFront;
}

// Attach to |client|'s tab whose title is |title|, and then attach to
// that tab's thread. Pass |callback| the thread attach response packet, a
// TargetFront referring to the tab, and a ThreadFront referring to the
// thread.
async function attachTestThread(client, title, callback = () => {}) {
  const targetFront = await attachTestTab(client, title);
  const threadFront = await targetFront.getFront("thread");
  const onPaused = threadFront.once("paused");
  await targetFront.attachThread({
    autoBlackBox: true,
  });
  const response = await onPaused;
  Assert.equal(threadFront.state, "paused", "Thread client is paused");
  Assert.ok("why" in response);
  Assert.equal(response.why.type, "attached");
  callback(response, targetFront, threadFront);
  return { targetFront, threadFront };
}

// Attach to |client|'s tab whose title is |title|, attach to the tab's
// thread, and then resume it. Pass |callback| the thread's response to
// the 'resume' packet, a TargetFront for the tab, and a ThreadFront for the
// thread.
async function attachTestTabAndResume(client, title, callback = () => {}) {
  const { targetFront, threadFront } = await attachTestThread(client, title);
  const response = await threadFront.resume();
  callback(response, targetFront, threadFront);
  return { targetFront, threadFront };
}

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer(server = DebuggerServer) {
  if (server === WorkerDebuggerServer) {
    const { createRootActor } = worker.require("xpcshell-test/testactors");
    server.setRootActor(createRootActor);
  } else {
    const { createRootActor } = require("xpcshell-test/testactors");
    server.setRootActor(createRootActor);
  }

  // Allow incoming connections.
  server.init(function() {
    return true;
  });
}

/**
 * Initialize the testing debugger server with a tab whose title is |title|.
 */
async function startTestDebuggerServer(title, server = DebuggerServer) {
  initTestDebuggerServer(server);
  addTestGlobal(title);
  DebuggerServer.registerActors({ target: true });

  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);

  await connect(client);
  return client;
}

async function finishClient(client) {
  await client.close();
  DebuggerServer.destroy();
  do_test_finished();
}

/**
 * Takes a relative file path and returns the absolute file url for it.
 */
function getFileUrl(name, allowMissing = false) {
  const file = do_get_file(name, allowMissing);
  return Services.io.newFileURI(file).spec;
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(
  name,
  allowMissing = false,
  usePlatformPathSeparator = false
) {
  const file = do_get_file(name, allowMissing);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci && file instanceof Ci.nsILocalFileWin) {
    filePrePath += "/";
  }

  path = path.slice(filePrePath.length);

  if (usePlatformPathSeparator && path.match(/^\w:/)) {
    path = path.replace(/\//g, "\\");
  }

  return path;
}

/**
 * Returns the full text contents of the given file.
 */
function readFile(fileName) {
  const f = do_get_file(fileName);
  const s = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  s.init(f, -1, -1, false);
  try {
    return NetUtil.readInputStreamToString(s, s.available());
  } finally {
    s.close();
  }
}

function writeFile(fileName, content) {
  const file = do_get_file(fileName, true);
  const stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  stream.init(file, -1, -1, 0);
  try {
    do {
      const numWritten = stream.write(content, content.length);
      content = content.slice(numWritten);
    } while (content.length > 0);
  } finally {
    stream.close();
  }
}

function connectPipeTracing() {
  return new TracingTransport(DebuggerServer.connectPipe());
}

function TracingTransport(childTransport) {
  this.hooks = null;
  this.child = childTransport;
  this.child.hooks = this;

  this.expectations = [];
  this.packets = [];
  this.checkIndex = 0;
}

TracingTransport.prototype = {
  // Remove actor names
  normalize: function(packet) {
    return JSON.parse(
      JSON.stringify(packet, (key, value) => {
        if (key === "to" || key === "from" || key === "actor") {
          return "<actorid>";
        }
        return value;
      })
    );
  },
  send: function(packet) {
    this.packets.push({
      type: "sent",
      packet: this.normalize(packet),
    });
    return this.child.send(packet);
  },
  close: function() {
    return this.child.close();
  },
  ready: function() {
    return this.child.ready();
  },
  onPacket: function(packet) {
    this.packets.push({
      type: "received",
      packet: this.normalize(packet),
    });
    this.hooks.onPacket(packet);
  },
  onClosed: function() {
    this.hooks.onClosed();
  },

  expectSend: function(expected) {
    const packet = this.packets[this.checkIndex++];
    Assert.equal(packet.type, "sent");
    deepEqual(packet.packet, this.normalize(expected));
  },

  expectReceive: function(expected) {
    const packet = this.packets[this.checkIndex++];
    Assert.equal(packet.type, "received");
    deepEqual(packet.packet, this.normalize(expected));
  },

  // Write your tests, call dumpLog at the end, inspect the output,
  // then sprinkle the calls through the right places in your test.
  dumpLog: function() {
    for (const entry of this.packets) {
      if (entry.type === "sent") {
        dumpn("trace.expectSend(" + entry.packet + ");");
      } else {
        dumpn("trace.expectReceive(" + entry.packet + ");");
      }
    }
  },
};

function StubTransport() {}
StubTransport.prototype.ready = function() {};
StubTransport.prototype.send = function() {};
StubTransport.prototype.close = function() {};

// Create async version of the object where calling each method
// is equivalent of calling it with asyncall. Mainly useful for
// destructuring objects with methods that take callbacks.
const Async = target => new Proxy(target, Async);
Async.get = (target, name) =>
  typeof target[name] === "function"
    ? asyncall.bind(null, target[name], target)
    : target[name];

// Calls async function that takes callback and errorback and returns
// returns promise representing result.
const asyncall = (fn, self, ...args) =>
  new Promise((...etc) => fn.call(self, ...args, ...etc));

const Test = task => () => {
  add_task(task);
  run_next_test();
};

const assert = Assert.ok.bind(Assert);

/**
 * Create a promise that is resolved on the next occurence of the given event.
 *
 * @param ThreadFront threadFront
 * @param String event
 * @param Function predicate
 * @returns Promise
 */
function waitForEvent(threadFront, type, predicate) {
  if (!predicate) {
    return threadFront.once(type);
  }

  return new Promise(function(resolve) {
    function listener(packet) {
      if (!predicate(packet)) {
        return;
      }
      threadFront.off(type, listener);
      resolve(packet);
    }
    threadFront.on(type, listener);
  });
}

/**
 * Execute the action on the next tick and return a promise that is resolved on
 * the next pause.
 *
 * When using promises and Task.jsm, we often want to do an action that causes a
 * pause and continue the task once the pause has ocurred. Unfortunately, if we
 * do the action that causes the pause within the task's current tick we will
 * pause before we have a chance to yield the promise that waits for the pause
 * and we enter a dead lock. The solution is to create the promise that waits
 * for the pause, schedule the action to run on the next tick of the event loop,
 * and finally yield the promise.
 *
 * @param Function action
 * @param ThreadFront threadFront
 * @returns Promise
 */
function executeOnNextTickAndWaitForPause(action, threadFront) {
  const paused = waitForPause(threadFront);
  executeSoon(action);
  return paused;
}

function evalCallback(debuggeeGlobal, func) {
  Cu.evalInSandbox("(" + func + ")()", debuggeeGlobal, "1.8", "test.js", 1);
}

/**
 * Interrupt JS execution for the specified thread.
 *
 * @param ThreadFront threadFront
 * @returns Promise
 */
function interrupt(threadFront) {
  dumpn("Interrupting.");
  return threadFront.interrupt();
}

/**
 * Resume JS execution for the specified thread and then wait for the next pause
 * event.
 *
 * @param DebuggerClient client
 * @param ThreadFront threadFront
 * @returns Promise
 */
function resumeAndWaitForPause(threadFront) {
  const paused = waitForPause(threadFront);
  return resume(threadFront).then(() => paused);
}

/**
 * Resume JS execution for a single step and wait for the pause after the step
 * has been taken.
 *
 * @param ThreadFront threadFront
 * @returns Promise
 */
function stepIn(threadFront) {
  dumpn("Stepping in.");
  const paused = waitForPause(threadFront);
  return threadFront.stepIn().then(() => paused);
}

/**
 * Resume JS execution for a step over and wait for the pause after the step
 * has been taken.
 *
 * @param ThreadFront threadFront
 * @returns Promise
 */
function stepOver(threadFront) {
  dumpn("Stepping over.");
  return threadFront.stepOver().then(() => waitForPause(threadFront));
}

/**
 * Resume JS execution for a step out and wait for the pause after the step
 * has been taken.
 *
 * @param DebuggerClient client
 * @param ThreadFront threadFront
 * @returns Promise
 */
function stepOut(threadFront) {
  dumpn("Stepping out.");
  return threadFront.stepOut().then(() => waitForPause(threadFront));
}

/**
 * Get the list of `count` frames currently on stack, starting at the index
 * `first` for the specified thread.
 *
 * @param ThreadFront threadFront
 * @param Number first
 * @param Number count
 * @returns Promise
 */
function getFrames(threadFront, first, count) {
  dumpn("Getting frames.");
  return threadFront.getFrames(first, count);
}

/**
 * Black box the specified source.
 *
 * @param SourceFront sourceFront
 * @returns Promise
 */
async function blackBox(sourceFront, range = null) {
  dumpn("Black boxing source: " + sourceFront.actor);
  const pausedInSource = await sourceFront.blackBox(range);
  ok(true, "blackBox didn't throw");
  return pausedInSource;
}

/**
 * Stop black boxing the specified source.
 *
 * @param SourceFront sourceFront
 * @returns Promise
 */
async function unBlackBox(sourceFront, range = null) {
  dumpn("Un-black boxing source: " + sourceFront.actor);
  await sourceFront.unblackBox(range);
  ok(true, "unblackBox didn't throw");
}

/**
 * Perform a "source" RDP request with the given SourceFront to get the source
 * content and content type.
 *
 * @param SourceFront sourceFront
 * @returns Promise
 */
function getSourceContent(sourceFront) {
  dumpn("Getting source content for " + sourceFront.actor);
  return sourceFront.source();
}

/**
 * Get a source at the specified url.
 *
 * @param ThreadFront threadFront
 * @param string url
 * @returns Promise<SourceFront>
 */
async function getSource(threadFront, url) {
  const source = await getSourceForm(threadFront, url);
  if (source) {
    return threadFront.source(source);
  }

  throw new Error("source not found");
}

async function getSourceById(threadFront, id) {
  const form = await getSourceFormById(threadFront, id);
  return threadFront.source(form);
}

async function getSourceForm(threadFront, url) {
  const { sources } = await threadFront.getSources();
  return sources.find(s => s.url === url);
}

async function getSourceFormById(threadFront, id) {
  const { sources } = await threadFront.getSources();
  return sources.find(source => source.actor == id);
}

/**
 * Do a reload which clears the thread debugger
 *
 * @param TabFront tabFront
 * @returns Promise<response>
 */
function reload(tabFront) {
  return tabFront.reload({});
}

/**
 * Returns an array of stack location strings given a thread and a sample.
 *
 * @param object thread
 * @param object sample
 * @returns object
 */
function getInflatedStackLocations(thread, sample) {
  const stackTable = thread.stackTable;
  const frameTable = thread.frameTable;
  const stringTable = thread.stringTable;
  const SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  const STACK_PREFIX_SLOT = stackTable.schema.prefix;
  const STACK_FRAME_SLOT = stackTable.schema.frame;
  const FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  const locations = [];
  while (stackIndex !== null) {
    const stackEntry = stackTable.data[stackIndex];
    const frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}

async function setupTestFromUrl(url) {
  do_test_pending();

  const { createRootActor } = require("xpcshell-test/testactors");
  DebuggerServer.setRootActor(createRootActor);
  DebuggerServer.init(() => true);

  const global = createTestGlobal("test");
  DebuggerServer.addTestGlobal(global);

  const debuggerClient = new DebuggerClient(DebuggerServer.connectPipe());
  await connect(debuggerClient);

  const tabs = await listTabs(debuggerClient);
  const targetFront = findTab(tabs, "test");
  await targetFront.attach();

  const [, threadFront] = await attachThread(targetFront);
  await resume(threadFront);

  const sourceUrl = getFileUrl(url);
  const promise = waitForNewSource(threadFront, sourceUrl);
  loadSubScript(sourceUrl, global);
  const { source } = await promise;

  const sourceFront = threadFront.source(source);
  return { global, debuggerClient, threadFront, sourceFront };
}

/**
 * Run the given test function twice, one with a regular DebuggerServer,
 * testing against a fake tab. And another one against a WorkerDebuggerServer,
 * testing the worker codepath.
 *
 * @param Function test
 *        Test function to run twice.
 *        This test function is called with a dictionary:
 *        - Sandbox debuggee
 *          The custom JS debuggee created for this test. This is a Sandbox using system
 *           principals by default.
 *        - ThreadFront threadFront
 *          A reference to a ThreadFront instance that is attached to the debuggee.
 *        - DebuggerClient client
 *          A reference to the DebuggerClient used to communicated with the RDP server.
 * @param Object options
 *        Optional arguments to tweak test environment
 *        - JSPrincipal principal
 *          Principal to use for the debuggee. Defaults to systemPrincipal.
 *        - boolean doNotRunWorker
 *          If true, do not run this tests in worker debugger context. Defaults to false.
 *        - bool wantXrays
 *          Whether the debuggee wants Xray vision with respect to same-origin objects
 *          outside the sandbox. Defaults to true.
 */
function threadFrontTest(test, options = {}) {
  const {
    principal = systemPrincipal,
    doNotRunWorker = false,
    wantXrays = true,
  } = options;

  async function runThreadFrontTestWithServer(server, test) {
    // Setup a server and connect a client to it.
    initTestDebuggerServer(server);

    // Create a custom debuggee and register it to the server.
    // We are using a custom Sandbox as debuggee. Create a new zone because
    // debugger and debuggee must be in different compartments.
    const debuggee = Cu.Sandbox(principal, { freshZone: true, wantXrays });
    const scriptName = "debuggee.js";
    debuggee.__name = scriptName;
    server.addTestGlobal(debuggee);

    const client = new DebuggerClient(server.connectPipe());
    await client.connect();

    // Attach to the fake tab target and retrieve the ThreadFront instance.
    // Automatically resume as the thread is paused by default after attach.
    const { targetFront, threadFront } = await attachTestTabAndResume(
      client,
      scriptName
    );

    // Run the test function
    await test({ threadFront, debuggee, client, server, targetFront });

    // Cleanup the client after the test ran
    await client.close();

    server.removeTestGlobal(debuggee);

    // Also cleanup the created server
    server.destroy();
  }

  return async () => {
    dump(">>> Run thread front test against a regular DebuggerServer\n");
    await runThreadFrontTestWithServer(DebuggerServer, test);

    // Skip tests that fail in the worker context
    if (!doNotRunWorker) {
      dump(">>> Run thread front test against a worker DebuggerServer\n");
      await runThreadFrontTestWithServer(WorkerDebuggerServer, test);
    }
  };
}
