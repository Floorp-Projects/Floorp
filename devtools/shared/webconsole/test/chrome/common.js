/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported attachConsole, attachConsoleToTab, attachConsoleToWorker,
   closeDebugger, checkConsoleAPICalls, checkRawHeaders, runTests, nextTest, Ci, Cc,
   withActiveServiceWorker, Services, consoleAPICall, createResourceWatcherForTab */

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { DevToolsServer } = require("devtools/server/devtools-server");
// eslint-disable-next-line mozilla/reject-some-requires
const { DevToolsClient } = require("devtools/client/devtools-client");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

const Services = require("Services");

function initCommon() {
  // Services.prefs.setBoolPref("devtools.debugger.log", true);
}

function initDevToolsServer() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;
}

async function connectToDebugger() {
  initCommon();
  initDevToolsServer();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);

  await client.connect();
  return client;
}

function attachConsole(listeners) {
  return _attachConsole(listeners);
}
function attachConsoleToTab(listeners) {
  return _attachConsole(listeners, true);
}
function attachConsoleToWorker(listeners) {
  return _attachConsole(listeners, true, true);
}

var _attachConsole = async function(listeners, attachToTab, attachToWorker) {
  try {
    const client = await connectToDebugger();

    function waitForMessage(target) {
      return new Promise(resolve => {
        target.addEventListener("message", resolve, { once: true });
      });
    }

    // Fetch the console actor out of the expected target
    // ParentProcessTarget / WorkerTarget / FrameTarget
    let target, worker;
    if (!attachToTab) {
      const targetDescriptor = await client.mainRoot.getMainProcess();
      target = await targetDescriptor.getTarget();
    } else {
      const targetDescriptor = await client.mainRoot.getTab();
      target = await targetDescriptor.getTarget();
      if (attachToWorker) {
        const workerName = "console-test-worker.js#" + new Date().getTime();
        worker = new Worker(workerName);
        await waitForMessage(worker);

        // listWorkers only works if the browsing context target actor is attached
        await target.attach();
        const { workers } = await target.listWorkers();
        target = workers.filter(w => w.url == workerName)[0];
        if (!target) {
          console.error(
            "listWorkers failed. Unable to find the worker actor\n"
          );
          return null;
        }
      }
    }

    // Attach the Target and the target thread in order to instantiate the console client.
    await target.attach();
    await target.attachThread();

    const webConsoleFront = await target.getFront("console");

    // By default the console isn't listening for anything,
    // request listeners from here
    const response = await webConsoleFront.startListeners(listeners);
    return {
      state: {
        dbgClient: client,
        webConsoleFront,
        actor: webConsoleFront.actor,
        // Keep a strong reference to the Worker to avoid it being
        // GCd during the test (bug 1237492).
        // eslint-disable-next-line camelcase
        _worker_ref: worker,
      },
      response,
    };
  } catch (error) {
    console.error(
      `attachConsole failed: ${error.error} ${error.message} - ` + error.stack
    );
  }
  return null;
};

async function createResourceWatcherForTab() {
  // Avoid mocha to try to load these module and fail while doing it when running node tests
  const {
    ResourceWatcher,
  } = require("devtools/shared/resources/resource-watcher");

  const commands = await CommandsFactory.forMainProcess();
  await commands.targetCommand.startListening();
  const target = commands.targetCommand.targetFront;
  const resourceWatcher = new ResourceWatcher(commands.targetCommand);

  return { resourceWatcher, target };
}

function closeDebugger(state, callback) {
  const onClose = state.dbgClient.close();

  state.dbgClient = null;
  state.client = null;

  if (typeof callback === "function") {
    onClose.then(callback);
  }
  return onClose;
}

function checkConsoleAPICalls(consoleCalls, expectedConsoleCalls) {
  is(
    consoleCalls.length,
    expectedConsoleCalls.length,
    "received correct number of console calls"
  );
  expectedConsoleCalls.forEach(function(message, index) {
    info("checking received console call #" + index);
    checkConsoleAPICall(consoleCalls[index], expectedConsoleCalls[index]);
  });
}

function checkConsoleAPICall(call, expected) {
  is(
    call.arguments?.length || 0,
    expected.arguments?.length || 0,
    "number of arguments"
  );

  checkObject(call, expected);
}

function checkObject(object, expected) {
  if (object && object.getGrip) {
    object = object.getGrip();
  }

  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue);
  }
}

function checkValue(name, value, expected) {
  if (expected === null) {
    ok(!value, "'" + name + "' is null");
  } else if (value === undefined) {
    ok(false, "'" + name + "' is undefined");
  } else if (value === null) {
    ok(false, "'" + name + "' is null");
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'");
  } else if (expected instanceof RegExp) {
    ok(expected.test(value), name + ": " + expected + " matched " + value);
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'");
    checkObject(value, expected);
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'");
    checkObject(value, expected);
  }
}

function checkHeadersOrCookies(array, expected) {
  const foundHeaders = {};

  for (const elem of array) {
    if (!(elem.name in expected)) {
      continue;
    }
    foundHeaders[elem.name] = true;
    info("checking value of header " + elem.name);
    checkValue(elem.name, elem.value, expected[elem.name]);
  }

  for (const header in expected) {
    if (!(header in foundHeaders)) {
      ok(false, header + " was not found");
    }
  }
}

function checkRawHeaders(text, expected) {
  const headers = text.split(/\r\n|\n|\r/);
  const arr = [];
  for (const header of headers) {
    const index = header.indexOf(": ");
    if (index < 0) {
      continue;
    }
    arr.push({
      name: header.substr(0, index),
      value: header.substr(index + 2),
    });
  }

  checkHeadersOrCookies(arr, expected);
}

var gTestState = {};

function runTests(tests, endCallback) {
  function* driver() {
    let lastResult, sendToNext;
    for (let i = 0; i < tests.length; i++) {
      gTestState.index = i;
      const fn = tests[i];
      info("will run test #" + i + ": " + fn.name);
      lastResult = fn(sendToNext, lastResult);
      sendToNext = yield lastResult;
    }
    yield endCallback(sendToNext, lastResult);
  }
  gTestState.driver = driver();
  return gTestState.driver.next();
}

function nextTest(message) {
  return gTestState.driver.next(message);
}

function withActiveServiceWorker(win, url, scope) {
  const opts = {};
  if (scope) {
    opts.scope = scope;
  }
  return win.navigator.serviceWorker.register(url, opts).then(swr => {
    if (swr.active) {
      return swr;
    }

    // Unfortunately we can't just use navigator.serviceWorker.ready promise
    // here.  If the service worker is for a scope that does not cover the window
    // then the ready promise will never resolve.  Instead monitor the service
    // workers state change events to determine when its activated.
    return new Promise(resolve => {
      const sw = swr.waiting || swr.installing;
      sw.addEventListener("statechange", function stateHandler(evt) {
        if (sw.state === "activated") {
          sw.removeEventListener("statechange", stateHandler);
          resolve(swr);
        }
      });
    });
  });
}

/**
 *
 * @param {Front} consoleFront
 * @param {Function} consoleCall: A function which calls the consoleAPI, e.g. :
 *                         `() => top.console.log("test")`.
 * @returns {Promise} A promise that will be resolved with the packet sent by the server
 *                    in response to the consoleAPI call.
 */
function consoleAPICall(consoleFront, consoleCall) {
  const onConsoleAPICall = consoleFront.once("consoleAPICall");
  consoleCall();
  return onConsoleAPICall;
}
