/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;
var CC = Components.Constructor;

const { require, loader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { worker } = Cu.import("resource://devtools/shared/worker/loader.js", {})
const promise = require("promise");
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const { promiseInvoke } = require("devtools/shared/async-utils");

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
const { MemoryFront } = require("devtools/server/actors/memory");

const { addDebuggerToGlobal } = Cu.import("resource://gre/modules/jsdebugger.jsm", {});

const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

var loadSubScript = Cc[
  '@mozilla.org/moz/jssubscript-loader;1'
].getService(Ci.mozIJSSubScriptLoader).loadSubScript;

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
          } catch(err) {
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
  let sandbox = Cu.Sandbox(
    Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
  );
  sandbox.__name = name;
  return sandbox;
}

function connect(client) {
  dump("Connecting client.\n");
  return new Promise(function (resolve) {
    client.connect(function () {
      resolve();
    });
  });
}

function close(client) {
  dump("Closing client.\n");
  return new Promise(function (resolve) {
    client.close(function () {
      resolve();
    });
  });
}

function listTabs(client) {
  dump("Listing tabs.\n");
  return rdpRequest(client, client.listTabs);
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
  return rdpRequest(client, client.attachTab, tab.actor);
}

function waitForNewSource(threadClient, url) {
  dump("Waiting for new source with url '" + url + "'.\n");
  return waitForEvent(threadClient, "newSource", function (packet) {
    return packet.source.url === url;
  });
}

function attachThread(tabClient, options = {}) {
  dump("Attaching to thread.\n");
  return rdpRequest(tabClient, tabClient.attachThread, options);
}

function resume(threadClient) {
  dump("Resuming thread.\n");
  return rdpRequest(threadClient, threadClient.resume);
}

function getSources(threadClient) {
  dump("Getting sources.\n");
  return rdpRequest(threadClient, threadClient.getSources);
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
  return rdpRequest(sourceClient, sourceClient.setBreakpoint, location);
}

function dumpn(msg) {
  dump("DBG-TEST: " + msg + "\n");
}

function tryImport(url) {
  try {
    Cu.import(url);
  } catch (e) {
    dumpn("Error importing " + url);
    dumpn(DevToolsUtils.safeErrorString(e));
    throw e;
  }
}

tryImport("resource://devtools/shared/Loader.jsm");
tryImport("resource://gre/modules/Console.jsm");

function testExceptionHook(ex) {
  try {
    do_report_unexpected_exception(ex);
  } catch(ex) {
    return {throw: ex}
  }
  return undefined;
}

// Convert an nsIScriptError 'aFlags' value into an appropriate string.
function scriptErrorFlagsToKind(aFlags) {
  var kind;
  if (aFlags & Ci.nsIScriptError.warningFlag)
    kind = "warning";
  if (aFlags & Ci.nsIScriptError.exceptionFlag)
    kind = "exception";
  else
    kind = "error";

  if (aFlags & Ci.nsIScriptError.strictFlag)
    kind = "strict " + kind;

  return kind;
}

// Redeclare dbg_assert with a fatal behavior.
function dbg_assert(cond, e) {
  if (!cond) {
    throw e;
  }
}

// Register a console listener, so console messages don't just disappear
// into the ether.
var errorCount = 0;
var listener = {
  observe: function (aMessage) {
    try {
      errorCount++;
      try {
        // If we've been given an nsIScriptError, then we can print out
        // something nicely formatted, for tools like Emacs to pick up.
        var scriptError = aMessage.QueryInterface(Ci.nsIScriptError);
        dumpn(aMessage.sourceName + ":" + aMessage.lineNumber + ": " +
              scriptErrorFlagsToKind(aMessage.flags) + ": " +
              aMessage.errorMessage);
        var string = aMessage.errorMessage;
      } catch (x) {
        // Be a little paranoid with message, as the whole goal here is to lose
        // no information.
        try {
          var string = "" + aMessage.message;
        } catch (x) {
          var string = "<error converting error message to string>";
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

function check_except(func)
{
  try {
    func();
  } catch (e) {
    do_check_true(true);
    return;
  }
  dumpn("Should have thrown an exception: " + func.toString());
  do_check_true(false);
}

function testGlobal(aName) {
  let systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
    .createInstance(Ci.nsIPrincipal);

  let sandbox = Cu.Sandbox(systemPrincipal);
  sandbox.__name = aName;
  return sandbox;
}

function addTestGlobal(aName, aServer = DebuggerServer)
{
  let global = testGlobal(aName);
  aServer.addTestGlobal(global);
  return global;
}

// List the DebuggerClient |aClient|'s tabs, look for one whose title is
// |aTitle|, and apply |aCallback| to the packet's entry for that tab.
function getTestTab(aClient, aTitle, aCallback) {
  aClient.listTabs(function (aResponse) {
    for (let tab of aResponse.tabs) {
      if (tab.title === aTitle) {
        aCallback(tab, aResponse);
        return;
      }
    }
    aCallback(null);
  });
}

// Attach to |aClient|'s tab whose title is |aTitle|; pass |aCallback| the
// response packet and a TabClient instance referring to that tab.
function attachTestTab(aClient, aTitle, aCallback) {
  getTestTab(aClient, aTitle, function (aTab) {
    aClient.attachTab(aTab.actor, aCallback);
  });
}

// Attach to |aClient|'s tab whose title is |aTitle|, and then attach to
// that tab's thread. Pass |aCallback| the thread attach response packet, a
// TabClient referring to the tab, and a ThreadClient referring to the
// thread.
function attachTestThread(aClient, aTitle, aCallback) {
  attachTestTab(aClient, aTitle, function (aTabResponse, aTabClient) {
    function onAttach(aResponse, aThreadClient) {
      aCallback(aResponse, aTabClient, aThreadClient, aTabResponse);
    }
    aTabClient.attachThread({
      useSourceMaps: true,
      autoBlackBox: true
    }, onAttach);
  });
}

// Attach to |aClient|'s tab whose title is |aTitle|, attach to the tab's
// thread, and then resume it. Pass |aCallback| the thread's response to
// the 'resume' packet, a TabClient for the tab, and a ThreadClient for the
// thread.
function attachTestTabAndResume(aClient, aTitle, aCallback) {
  attachTestThread(aClient, aTitle, function(aResponse, aTabClient, aThreadClient) {
    aThreadClient.resume(function (aResponse) {
      aCallback(aResponse, aTabClient, aThreadClient);
    });
  });
}

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer(aServer = DebuggerServer)
{
  aServer.registerModule("xpcshell-test/testactors");
  // Allow incoming connections.
  aServer.init(function () { return true; });
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

function finishClient(aClient)
{
  aClient.close(function() {
    DebuggerServer.destroy();
    do_test_finished();
  });
}

// Create a server, connect to it and fetch tab actors for the parent process;
// pass |aCallback| the debugger client and tab actor form with all actor IDs.
function get_chrome_actors(callback)
{
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.getProcess().then(response => {
      callback(client, response.form);
    });
  });
}

function getChromeActors(client, server = DebuggerServer) {
  server.allowChromeProcess = true;
  return client.getProcess().then(response => response.form);
}

/**
 * Takes a relative file path and returns the absolute file url for it.
 */
function getFileUrl(aName, aAllowMissing=false) {
  let file = do_get_file(aName, aAllowMissing);
  return Services.io.newFileURI(file).spec;
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(aName, aAllowMissing=false, aUsePlatformPathSeparator=false)
{
  let file = do_get_file(aName, aAllowMissing);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci &&
      file instanceof Ci.nsILocalFileWin) {
    filePrePath += "/";
  }

  path = path.slice(filePrePath.length);

  if (aUsePlatformPathSeparator && path.match(/^\w:/)) {
    path = path.replace(/\//g, "\\");
  }

  return path;
}

Cu.import("resource://gre/modules/NetUtil.jsm");

/**
 * Returns the full text contents of the given file.
 */
function readFile(aFileName) {
  let f = do_get_file(aFileName);
  let s = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  s.init(f, -1, -1, false);
  try {
    return NetUtil.readInputStreamToString(s, s.available());
  } finally {
    s.close();
  }
}

function writeFile(aFileName, aContent) {
  let file = do_get_file(aFileName, true);
  let stream = Cc["@mozilla.org/network/file-output-stream;1"]
    .createInstance(Ci.nsIFileOutputStream);
  stream.init(file, -1, -1, 0);
  try {
    do {
      let numWritten = stream.write(aContent, aContent.length);
      aContent = aContent.slice(numWritten);
    } while (aContent.length > 0);
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
    return JSON.parse(JSON.stringify(packet, (key, value) => {
      if (key === "to" || key === "from" || key === "actor") {
        return "<actorid>";
      }
      return value;
    }));
  },
  send: function(packet) {
    this.packets.push({
      type: "sent",
      packet: this.normalize(packet)
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
      packet: this.normalize(packet)
    });
    this.hooks.onPacket(packet);
  },
  onClosed: function() {
    this.hooks.onClosed();
  },

  expectSend: function(expected) {
    let packet = this.packets[this.checkIndex++];
    do_check_eq(packet.type, "sent");
    deepEqual(packet.packet, this.normalize(expected));
  },

  expectReceive: function(expected) {
    let packet = this.packets[this.checkIndex++];
    do_check_eq(packet.type, "received");
    deepEqual(packet.packet, this.normalize(expected));
  },

  // Write your tests, call dumpLog at the end, inspect the output,
  // then sprinkle the calls through the right places in your test.
  dumpLog: function() {
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
StubTransport.prototype.send  = function () {};
StubTransport.prototype.close = function () {};

function executeSoon(aFunc) {
  Services.tm.mainThread.dispatch({
    run: DevToolsUtils.makeInfallible(aFunc)
  }, Ci.nsIThread.DISPATCH_NORMAL);
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
  typeof(target[name]) === "function" ? asyncall.bind(null, target[name], target) :
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
 * Create a promise that is resolved with the server's response to the client's
 * Remote Debugger Protocol request. If a response with the `error` property is
 * received, the promise is rejected. Any extra arguments passed in are
 * forwarded to the method invocation.
 *
 * See `setBreakpoint` below, for example usage.
 *
 * @param DebuggerClient/ThreadClient/SourceClient/etc client
 * @param Function method
 * @param any args
 * @returns Promise
 */
function rdpRequest(client, method, ...args) {
  return promiseInvoke(client, method, ...args)
    .then(response => {
      const { error, message } = response;
      if (error) {
        throw new Error(error + ": " + message);
      }
      return response;
    });
}

/**
 * Interrupt JS execution for the specified thread.
 *
 * @param ThreadClient threadClient
 * @returns Promise
 */
function interrupt(threadClient) {
  dumpn("Interrupting.");
  return rdpRequest(threadClient, threadClient.interrupt);
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
  return rdpRequest(threadClient, threadClient.stepIn)
    .then(() => paused);
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
  return rdpRequest(threadClient, threadClient.getFrames, first, count);
}

/**
 * Black box the specified source.
 *
 * @param SourceClient sourceClient
 * @returns Promise
 */
function blackBox(sourceClient) {
  dumpn("Black boxing source: " + sourceClient.actor);
  return rdpRequest(sourceClient, sourceClient.blackBox);
}

/**
 * Stop black boxing the specified source.
 *
 * @param SourceClient sourceClient
 * @returns Promise
 */
function unBlackBox(sourceClient) {
  dumpn("Un-black boxing source: " + sourceClient.actor);
  return rdpRequest(sourceClient, sourceClient.unblackBox);
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
  return rdpRequest(sourceClient, sourceClient.source);
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
    let source = res.sources.filter(function(s) {
      return s.url === url;
    });
    if (source.length) {
      deferred.resolve(threadClient.source(source[0]));
    }
    else {
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
