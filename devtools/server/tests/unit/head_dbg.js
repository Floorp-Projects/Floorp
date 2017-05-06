/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: ["error", {"vars": "local"}] */
/* eslint-disable no-shadow */

"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;
var CC = Components.Constructor;

// Populate AppInfo before anything (like the shared loader) accesses
// System.appinfo, which is a lazy getter.
const _appInfo = {};
Cu.import("resource://testing-common/AppInfo.jsm", _appInfo);
_appInfo.updateAppInfo({
  ID: "devtools@tests.mozilla.org",
  name: "devtools-tests",
  version: "1",
  platformVersion: "42",
  crashReporter: true,
});

const { require, loader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { worker } = Cu.import("resource://devtools/shared/worker/loader.js", {});
const promise = require("promise");
const { Task } = require("devtools/shared/task");
const { console } = require("resource://gre/modules/Console.jsm");
const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");

const Services = require("Services");
// Always log packets when running tests. runxpcshelltests.py will throw
// the output away anyway, unless you give it the --verbose flag.
Services.prefs.setBoolPref("devtools.debugger.log", true);
// Enable remote debugging for the relevant tests.
Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { DebuggerServer } = require("devtools/server/main");
const { DebuggerServer: WorkerDebuggerServer } = worker.require("devtools/server/main");
const { DebuggerClient, ObjectClient } = require("devtools/shared/client/main");
const { MemoryFront } = require("devtools/shared/fronts/memory");

const { addDebuggerToGlobal } = Cu.import("resource://gre/modules/jsdebugger.jsm", {});

const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
                        .createInstance(Ci.nsIPrincipal);

var { loadSubScript } = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                        .getService(Ci.mozIJSSubScriptLoader);

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

/**
 * Create a `run_test` function that runs the given generator in a task after
 * having attached to a memory actor. When done, the memory actor is detached
 * from, the client is finished, and the test is finished.
 *
 * @param {GeneratorFunction} testGeneratorFunction
 *        The generator function is passed (DebuggerClient, MemoryFront)
 *        arguments.
 *
 * @returns `run_test` function
 */
function makeMemoryActorTest(testGeneratorFunction) {
  const TEST_GLOBAL_NAME = "test_MemoryActor";

  return function run_test() {
    do_test_pending();
    startTestDebuggerServer(TEST_GLOBAL_NAME).then(client => {
      DebuggerServer.registerModule("devtools/server/actors/heap-snapshot-file", {
        prefix: "heapSnapshotFile",
        constructor: "HeapSnapshotFileActor",
        type: { global: true }
      });

      getTestTab(client, TEST_GLOBAL_NAME, function (tabForm, rootForm) {
        if (!tabForm || !rootForm) {
          ok(false, "Could not attach to test tab: " + TEST_GLOBAL_NAME);
          return;
        }

        Task.spawn(function* () {
          try {
            const memoryFront = new MemoryFront(client, tabForm, rootForm);
            yield memoryFront.attach();
            yield* testGeneratorFunction(client, memoryFront);
            yield memoryFront.detach();
          } catch (err) {
            DevToolsUtils.reportException("makeMemoryActorTest", err);
            ok(false, "Got an error: " + err);
          }

          finishClient(client);
        });
      });
    });
  };
}

/**
 * Save as makeMemoryActorTest but attaches the MemoryFront to the MemoryActor
 * scoped to the full runtime rather than to a tab.
 */
function makeFullRuntimeMemoryActorTest(testGeneratorFunction) {
  return function run_test() {
    do_test_pending();
    startTestDebuggerServer("test_MemoryActor").then(client => {
      DebuggerServer.registerModule("devtools/server/actors/heap-snapshot-file", {
        prefix: "heapSnapshotFile",
        constructor: "HeapSnapshotFileActor",
        type: { global: true }
      });

      getChromeActors(client).then(function (form) {
        if (!form) {
          ok(false, "Could not attach to chrome actors");
          return;
        }

        Task.spawn(function* () {
          try {
            const rootForm = yield listTabs(client);
            const memoryFront = new MemoryFront(client, form, rootForm);
            yield memoryFront.attach();
            yield* testGeneratorFunction(client, memoryFront);
            yield memoryFront.detach();
          } catch (err) {
            DevToolsUtils.reportException("makeMemoryActorTest", err);
            ok(false, "Got an error: " + err);
          }

          finishClient(client);
        });
      });
    });
  };
}

function createTestGlobal(name) {
  let sandbox = Cu.Sandbox(Cc["@mozilla.org/systemprincipal;1"]
                           .createInstance(Ci.nsIPrincipal));
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
  return client.listTabs();
}

function findTab(tabs, title) {
  dump("Finding tab with title '" + title + "'.\n");
  for (let tab of tabs) {
    if (tab.title === title) {
      return tab;
    }
  }
  return null;
}

function attachTab(client, tab) {
  dump("Attaching to tab with title '" + tab.title + "'.\n");
  return client.attachTab(tab.actor);
}

function waitForNewSource(threadClient, url) {
  dump("Waiting for new source with url '" + url + "'.\n");
  return waitForEvent(threadClient, "newSource", function (packet) {
    return packet.source.url === url;
  });
}

function attachThread(tabClient, options = {}) {
  dump("Attaching to thread.\n");
  return tabClient.attachThread(options);
}

function resume(threadClient) {
  dump("Resuming thread.\n");
  return threadClient.resume();
}

function getSources(threadClient) {
  dump("Getting sources.\n");
  return threadClient.getSources();
}

function findSource(sources, url) {
  dump("Finding source with url '" + url + "'.\n");
  for (let source of sources) {
    if (source.url === url) {
      return source;
    }
  }
  return null;
}

function waitForPause(threadClient) {
  dump("Waiting for pause.\n");
  return waitForEvent(threadClient, "paused");
}

function setBreakpoint(sourceClient, location) {
  dump("Setting breakpoint.\n");
  return sourceClient.setBreakpoint(location);
}

function dumpn(msg) {
  dump("DBG-TEST: " + msg + "\n");
}

function testExceptionHook(ex) {
  try {
    do_report_unexpected_exception(ex);
  } catch (e) {
    return {throw: e};
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
  observe: function (message) {
    try {
      let string;
      errorCount++;
      try {
        // If we've been given an nsIScriptError, then we can print out
        // something nicely formatted, for tools like Emacs to pick up.
        message.QueryInterface(Ci.nsIScriptError);
        dumpn(message.sourceName + ":" + message.lineNumber + ": " +
              scriptErrorFlagsToKind(message.flags) + ": " +
              message.errorMessage);
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
      while (DebuggerServer
             && DebuggerServer.xpcInspector
             && DebuggerServer.xpcInspector.eventLoopNestLevel > 0) {
        DebuggerServer.xpcInspector.exitNestedEventLoop();
      }

      // In the world before bug 997440, exceptions were getting lost because of
      // the arbitrary JSContext being used in nsXPCWrappedJSClass::CallMethod.
      // In the new world, the wanderers have returned. However, because of the,
      // currently very-broken, exception reporting machinery in
      // XPCWrappedJSClass these get reported as errors to the console, even if
      // there's actually JS on the stack above that will catch them.  If we
      // throw an error here because of them our tests start failing.  So, we'll
      // just dump the message to the logs instead, to make sure the information
      // isn't lost.
      dumpn("head_dbg.js observed a console message: " + string);
    } catch (_) {
      // Swallow everything to avoid console reentrancy errors. We did our best
      // to log above, but apparently that didn't cut it.
    }
  }
};

var consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
consoleService.registerListener(listener);

function check_except(func) {
  try {
    func();
  } catch (e) {
    do_check_true(true);
    return;
  }
  dumpn("Should have thrown an exception: " + func.toString());
  do_check_true(false);
}

function testGlobal(name) {
  let sandbox = Cu.Sandbox(Cc["@mozilla.org/systemprincipal;1"]
                           .createInstance(Ci.nsIPrincipal));
  sandbox.__name = name;
  return sandbox;
}

function addTestGlobal(name, server = DebuggerServer) {
  let global = testGlobal(name);
  server.addTestGlobal(global);
  return global;
}

// List the DebuggerClient |client|'s tabs, look for one whose title is
// |title|, and apply |callback| to the packet's entry for that tab.
function getTestTab(client, title, callback) {
  client.listTabs(function (response) {
    for (let tab of response.tabs) {
      if (tab.title === title) {
        callback(tab, response);
        return;
      }
    }
    callback(null);
  });
}

// Attach to |client|'s tab whose title is |title|; pass |callback| the
// response packet and a TabClient instance referring to that tab.
function attachTestTab(client, title, callback) {
  getTestTab(client, title, function (tab) {
    client.attachTab(tab.actor, callback);
  });
}

// Attach to |client|'s tab whose title is |title|, and then attach to
// that tab's thread. Pass |callback| the thread attach response packet, a
// TabClient referring to the tab, and a ThreadClient referring to the
// thread.
function attachTestThread(client, title, callback) {
  attachTestTab(client, title, function (tabResponse, tabClient) {
    function onAttach(response, threadClient) {
      callback(response, tabClient, threadClient, tabResponse);
    }
    tabClient.attachThread({
      useSourceMaps: true,
      autoBlackBox: true
    }, onAttach);
  });
}

// Attach to |client|'s tab whose title is |title|, attach to the tab's
// thread, and then resume it. Pass |callback| the thread's response to
// the 'resume' packet, a TabClient for the tab, and a ThreadClient for the
// thread.
function attachTestTabAndResume(client, title, callback = () => {}) {
  return new Promise((resolve) => {
    attachTestThread(client, title, function (response, tabClient, threadClient) {
      threadClient.resume(function (response) {
        callback(response, tabClient, threadClient);
        resolve([response, tabClient, threadClient]);
      });
    });
  });
}

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer(server = DebuggerServer) {
  server.registerModule("xpcshell-test/testactors");
  // Allow incoming connections.
  server.init(function () {
    return true;
  });
}

/**
 * Initialize the testing debugger server with a tab whose title is |title|.
 */
function startTestDebuggerServer(title, server = DebuggerServer) {
  initTestDebuggerServer(server);
  addTestGlobal(title);
  DebuggerServer.addTabActors();

  let transport = DebuggerServer.connectPipe();
  let client = new DebuggerClient(transport);

  return connect(client).then(() => client);
}

function finishClient(client) {
  client.close(function () {
    DebuggerServer.destroy();
    do_test_finished();
  });
}

// Create a server, connect to it and fetch tab actors for the parent process;
// pass |callback| the debugger client and tab actor form with all actor IDs.
function get_chrome_actors(callback) {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect()
    .then(() => client.getProcess())
    .then(response => {
      callback(client, response.form);
    });
}

function getChromeActors(client, server = DebuggerServer) {
  server.allowChromeProcess = true;
  return client.getProcess().then(response => response.form);
}

/**
 * Takes a relative file path and returns the absolute file url for it.
 */
function getFileUrl(name, allowMissing = false) {
  let file = do_get_file(name, allowMissing);
  return Services.io.newFileURI(file).spec;
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(name, allowMissing = false, usePlatformPathSeparator = false) {
  let file = do_get_file(name, allowMissing);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci &&
      file instanceof Ci.nsILocalFileWin) {
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
  let f = do_get_file(fileName);
  let s = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  s.init(f, -1, -1, false);
  try {
    return NetUtil.readInputStreamToString(s, s.available());
  } finally {
    s.close();
  }
}

function writeFile(fileName, content) {
  let file = do_get_file(fileName, true);
  let stream = Cc["@mozilla.org/network/file-output-stream;1"]
    .createInstance(Ci.nsIFileOutputStream);
  stream.init(file, -1, -1, 0);
  try {
    do {
      let numWritten = stream.write(content, content.length);
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
  normalize: function (packet) {
    return JSON.parse(JSON.stringify(packet, (key, value) => {
      if (key === "to" || key === "from" || key === "actor") {
        return "<actorid>";
      }
      return value;
    }));
  },
  send: function (packet) {
    this.packets.push({
      type: "sent",
      packet: this.normalize(packet)
    });
    return this.child.send(packet);
  },
  close: function () {
    return this.child.close();
  },
  ready: function () {
    return this.child.ready();
  },
  onPacket: function (packet) {
    this.packets.push({
      type: "received",
      packet: this.normalize(packet)
    });
    this.hooks.onPacket(packet);
  },
  onClosed: function () {
    this.hooks.onClosed();
  },

  expectSend: function (expected) {
    let packet = this.packets[this.checkIndex++];
    do_check_eq(packet.type, "sent");
    deepEqual(packet.packet, this.normalize(expected));
  },

  expectReceive: function (expected) {
    let packet = this.packets[this.checkIndex++];
    do_check_eq(packet.type, "received");
    deepEqual(packet.packet, this.normalize(expected));
  },

  // Write your tests, call dumpLog at the end, inspect the output,
  // then sprinkle the calls through the right places in your test.
  dumpLog: function () {
    for (let entry of this.packets) {
      if (entry.type === "sent") {
        dumpn("trace.expectSend(" + entry.packet + ");");
      } else {
        dumpn("trace.expectReceive(" + entry.packet + ");");
      }
    }
  }
};

function StubTransport() { }
StubTransport.prototype.ready = function () {};
StubTransport.prototype.send = function () {};
StubTransport.prototype.close = function () {};

function executeSoon(func) {
  Services.tm.dispatchToMainThread({
    run: DevToolsUtils.makeInfallible(func)
  });
}

// The do_check_* family of functions expect their last argument to be an
// optional stack object. Unfortunately, most tests actually pass a in a string
// containing an error message instead, which causes error reporting to break if
// strict warnings as errors is turned on. To avoid this, we wrap these
// functions here below to ensure the correct number of arguments is passed.
//
// TODO: Remove this once bug 906232 is resolved
//
var do_check_true_old = do_check_true;
var do_check_true = function (condition) {
  do_check_true_old(condition);
};

var do_check_false_old = do_check_false;
var do_check_false = function (condition) {
  do_check_false_old(condition);
};

var do_check_eq_old = do_check_eq;
var do_check_eq = function (left, right) {
  do_check_eq_old(left, right);
};

var do_check_neq_old = do_check_neq;
var do_check_neq = function (left, right) {
  do_check_neq_old(left, right);
};

var do_check_matches_old = do_check_matches;
var do_check_matches = function (pattern, value) {
  do_check_matches_old(pattern, value);
};

// Create async version of the object where calling each method
// is equivalent of calling it with asyncall. Mainly useful for
// destructuring objects with methods that take callbacks.
const Async = target => new Proxy(target, Async);
Async.get = (target, name) =>
  typeof (target[name]) === "function" ? asyncall.bind(null, target[name], target) :
  target[name];

// Calls async function that takes callback and errorback and returns
// returns promise representing result.
const asyncall = (fn, self, ...args) =>
  new Promise((...etc) => fn.call(self, ...args, ...etc));

const Test = task => () => {
  add_task(task);
  run_next_test();
};

const assert = do_check_true;

/**
 * Create a promise that is resolved on the next occurence of the given event.
 *
 * @param DebuggerClient client
 * @param String event
 * @param Function predicate
 * @returns Promise
 */
function waitForEvent(client, type, predicate) {
  return new Promise(function (resolve) {
    function listener(type, packet) {
      if (!predicate(packet)) {
        return;
      }
      client.removeListener(listener);
      resolve(packet);
    }

    if (predicate) {
      client.addListener(type, listener);
    } else {
      client.addOneTimeListener(type, function (type, packet) {
        resolve(packet);
      });
    }
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
 * @param DebuggerClient client
 * @returns Promise
 */
function executeOnNextTickAndWaitForPause(action, client) {
  const paused = waitForPause(client);
  executeSoon(action);
  return paused;
}

/**
 * Interrupt JS execution for the specified thread.
 *
 * @param ThreadClient threadClient
 * @returns Promise
 */
function interrupt(threadClient) {
  dumpn("Interrupting.");
  return threadClient.interrupt();
}

/**
 * Resume JS execution for the specified thread and then wait for the next pause
 * event.
 *
 * @param DebuggerClient client
 * @param ThreadClient threadClient
 * @returns Promise
 */
function resumeAndWaitForPause(client, threadClient) {
  const paused = waitForPause(client);
  return resume(threadClient).then(() => paused);
}

/**
 * Resume JS execution for a single step and wait for the pause after the step
 * has been taken.
 *
 * @param DebuggerClient client
 * @param ThreadClient threadClient
 * @returns Promise
 */
function stepIn(client, threadClient) {
  dumpn("Stepping in.");
  const paused = waitForPause(client);
  return threadClient.stepIn()
    .then(() => paused);
}

/**
 * Resume JS execution for a step over and wait for the pause after the step
 * has been taken.
 *
 * @param DebuggerClient client
 * @param ThreadClient threadClient
 * @returns Promise
 */
function stepOver(client, threadClient) {
  dumpn("Stepping over.");
  return threadClient.stepOver()
    .then(() => waitForPause(client));
}

/**
 * Get the list of `count` frames currently on stack, starting at the index
 * `first` for the specified thread.
 *
 * @param ThreadClient threadClient
 * @param Number first
 * @param Number count
 * @returns Promise
 */
function getFrames(threadClient, first, count) {
  dumpn("Getting frames.");
  return threadClient.getFrames(first, count);
}

/**
 * Black box the specified source.
 *
 * @param SourceClient sourceClient
 * @returns Promise
 */
function blackBox(sourceClient) {
  dumpn("Black boxing source: " + sourceClient.actor);
  return sourceClient.blackBox();
}

/**
 * Stop black boxing the specified source.
 *
 * @param SourceClient sourceClient
 * @returns Promise
 */
function unBlackBox(sourceClient) {
  dumpn("Un-black boxing source: " + sourceClient.actor);
  return sourceClient.unblackBox();
}

/**
 * Perform a "source" RDP request with the given SourceClient to get the source
 * content and content type.
 *
 * @param SourceClient sourceClient
 * @returns Promise
 */
function getSourceContent(sourceClient) {
  dumpn("Getting source content for " + sourceClient.actor);
  return sourceClient.source();
}

/**
 * Get a source at the specified url.
 *
 * @param ThreadClient threadClient
 * @param string url
 * @returns Promise<SourceClient>
 */
function getSource(threadClient, url) {
  let deferred = promise.defer();
  threadClient.getSources((res) => {
    let source = res.sources.filter(function (s) {
      return s.url === url;
    });
    if (source.length) {
      deferred.resolve(threadClient.source(source[0]));
    } else {
      deferred.reject(new Error("source not found"));
    }
  });
  return deferred.promise;
}

/**
 * Do a fake reload which clears the thread debugger
 *
 * @param TabClient tabClient
 * @returns Promise<response>
 */
function reload(tabClient) {
  let deferred = promise.defer();
  tabClient._reload({}, deferred.resolve);
  return deferred.promise;
}

/**
 * Returns an array of stack location strings given a thread and a sample.
 *
 * @param object thread
 * @param object sample
 * @returns object
 */
function getInflatedStackLocations(thread, sample) {
  let stackTable = thread.stackTable;
  let frameTable = thread.frameTable;
  let stringTable = thread.stringTable;
  let SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  let STACK_PREFIX_SLOT = stackTable.schema.prefix;
  let STACK_FRAME_SLOT = stackTable.schema.frame;
  let FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  let locations = [];
  while (stackIndex !== null) {
    let stackEntry = stackTable.data[stackIndex];
    let frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}
