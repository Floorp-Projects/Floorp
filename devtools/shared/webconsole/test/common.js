/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported ObjectClient, attachConsole, attachConsoleToTab, attachConsoleToWorker,
   closeDebugger, checkConsoleAPICalls, checkRawHeaders, runTests, nextTest, Ci, Cc,
   withActiveServiceWorker, Services */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
// This gives logging to stdout for tests
const {console} = Cu.import("resource://gre/modules/Console.jsm", {});
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {Task} = require("devtools/shared/task");
const {DebuggerServer} = require("devtools/server/main");
const {DebuggerClient} = require("devtools/shared/client/debugger-client");
const ObjectClient = require("devtools/shared/client/object-client");
const Services = require("Services");

function initCommon() {
  // Services.prefs.setBoolPref("devtools.debugger.log", true);
}

function initDebuggerServer() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;
}

function connectToDebugger() {
  initCommon();
  initDebuggerServer();

  let transport = DebuggerServer.connectPipe();
  let client = new DebuggerClient(transport);

  let dbgState = { dbgClient: client };
  return new Promise(resolve => {
    client.connect().then(response => resolve([dbgState, response]));
  });
}

function attachConsole(listeners, callback) {
  _attachConsole(listeners, callback);
}
function attachConsoleToTab(listeners, callback) {
  _attachConsole(listeners, callback, true);
}
function attachConsoleToWorker(listeners, callback) {
  _attachConsole(listeners, callback, true, true);
}

var _attachConsole = Task.async(function* (
  listeners, callback, attachToTab, attachToWorker
) {
  function _onAttachConsole(state, response, webConsoleClient) {
    if (response.error) {
      console.error("attachConsole failed: " + response.error + " " +
                    response.message);
    }

    state.client = webConsoleClient;

    callback(state, response);
  }

  function waitForMessage(target) {
    return new Promise(resolve => {
      target.addEventListener("message", resolve, { once: true });
    });
  }

  let [state, response] = yield connectToDebugger();
  if (response.error) {
    console.error("client.connect() failed: " + response.error + " " +
                  response.message);
    callback(state, response);
    return;
  }

  if (!attachToTab) {
    response = yield state.dbgClient.getProcess();
    yield state.dbgClient.attachTab(response.form.actor);
    let consoleActor = response.form.consoleActor;
    state.actor = consoleActor;
    state.dbgClient.attachConsole(consoleActor, listeners,
                                  _onAttachConsole.bind(null, state));
    return;
  }
  response = yield state.dbgClient.listTabs();
  if (response.error) {
    console.error("listTabs failed: " + response.error + " " +
                  response.message);
    callback(state, response);
    return;
  }
  let tab = response.tabs[response.selected];
  let [, tabClient] = yield state.dbgClient.attachTab(tab.actor);
  if (attachToWorker) {
    let workerName = "console-test-worker.js#" + new Date().getTime();
    let worker = new Worker(workerName);
    // Keep a strong reference to the Worker to avoid it being
    // GCd during the test (bug 1237492).
    // eslint-disable-next-line camelcase
    state._worker_ref = worker;
    yield waitForMessage(worker);

    let { workers } = yield tabClient.listWorkers();
    let workerActor = workers.filter(w => w.url == workerName)[0].actor;
    if (!workerActor) {
      console.error("listWorkers failed. Unable to find the " +
                    "worker actor\n");
      return;
    }
    let [workerResponse, workerClient] = yield tabClient.attachWorker(workerActor);
    if (!workerClient || workerResponse.error) {
      console.error("attachWorker failed. No worker client or " +
                    " error: " + workerResponse.error);
      return;
    }
    yield workerClient.attachThread({});
    state.actor = workerClient.consoleActor;
    state.dbgClient.attachConsole(workerClient.consoleActor, listeners,
                                  _onAttachConsole.bind(null, state));
  } else {
    state.actor = tab.consoleActor;
    state.dbgClient.attachConsole(tab.consoleActor, listeners,
                                   _onAttachConsole.bind(null, state));
  }
});

function closeDebugger(state, callback) {
  state.dbgClient.close().then(callback);
  state.dbgClient = null;
  state.client = null;
}

function checkConsoleAPICalls(consoleCalls, expectedConsoleCalls) {
  is(consoleCalls.length, expectedConsoleCalls.length,
    "received correct number of console calls");
  expectedConsoleCalls.forEach(function (message, index) {
    info("checking received console call #" + index);
    checkConsoleAPICall(consoleCalls[index], expectedConsoleCalls[index]);
  });
}

function checkConsoleAPICall(call, expected) {
  if (expected.level != "trace" && expected.arguments) {
    is(call.arguments.length, expected.arguments.length,
       "number of arguments");
  }

  checkObject(call, expected);
}

function checkObject(object, expected) {
  for (let name of Object.keys(expected)) {
    let expectedValue = expected[name];
    let value = object[name];
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
  } else if (typeof expected == "string" || typeof expected == "number" ||
             typeof expected == "boolean") {
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
  let foundHeaders = {};

  for (let elem of array) {
    if (!(elem.name in expected)) {
      continue;
    }
    foundHeaders[elem.name] = true;
    info("checking value of header " + elem.name);
    checkValue(elem.name, elem.value, expected[elem.name]);
  }

  for (let header in expected) {
    if (!(header in foundHeaders)) {
      ok(false, header + " was not found");
    }
  }
}

function checkRawHeaders(text, expected) {
  let headers = text.split(/\r\n|\n|\r/);
  let arr = [];
  for (let header of headers) {
    let index = header.indexOf(": ");
    if (index < 0) {
      continue;
    }
    arr.push({
      name: header.substr(0, index),
      value: header.substr(index + 2)
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
      let fn = tests[i];
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
  let opts = {};
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
      let sw = swr.waiting || swr.installing;
      sw.addEventListener("statechange", function stateHandler(evt) {
        if (sw.state === "activated") {
          sw.removeEventListener("statechange", stateHandler);
          resolve(swr);
        }
      });
    });
  });
}
