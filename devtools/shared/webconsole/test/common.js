/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const XHTML_NS = "http://www.w3.org/1999/xhtml";

// This gives logging to stdout for tests
var {console} = Cu.import("resource://gre/modules/Console.jsm", {});

var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
var Services = require("Services");
var WebConsoleUtils = require("devtools/client/webconsole/utils").Utils;
var {Task} = require("devtools/shared/task");

var ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                          .getService(Ci.nsIConsoleAPIStorage);
var {DebuggerServer} = require("devtools/server/main");
var {DebuggerClient, ObjectClient} = require("devtools/shared/client/main");

var {ConsoleServiceListener, ConsoleAPIListener} =
  require("devtools/server/actors/utils/webconsole-listeners");

function initCommon()
{
  // Services.prefs.setBoolPref("devtools.debugger.log", true);
}

function initDebuggerServer()
{
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;
}

function connectToDebugger(aCallback)
{
  initCommon();
  initDebuggerServer();

  let transport = DebuggerServer.connectPipe();
  let client = new DebuggerClient(transport);

  let dbgState = { dbgClient: client };
  client.connect().then(response => aCallback(dbgState, response));
}

function attachConsole(aListeners, aCallback) {
  _attachConsole(aListeners, aCallback);
}
function attachConsoleToTab(aListeners, aCallback) {
  _attachConsole(aListeners, aCallback, true);
}
function attachConsoleToWorker(aListeners, aCallback) {
  _attachConsole(aListeners, aCallback, true, true);
}

function _attachConsole(aListeners, aCallback, aAttachToTab, aAttachToWorker)
{
  function _onAttachConsole(aState, aResponse, aWebConsoleClient)
  {
    if (aResponse.error) {
      console.error("attachConsole failed: " + aResponse.error + " " +
                    aResponse.message);
    }

    aState.client = aWebConsoleClient;

    aCallback(aState, aResponse);
  }

  connectToDebugger(function _onConnect(aState, aResponse) {
    if (aResponse.error) {
      console.error("client.connect() failed: " + aResponse.error + " " +
                    aResponse.message);
      aCallback(aState, aResponse);
      return;
    }

    if (aAttachToTab) {
      aState.dbgClient.listTabs(function _onListTabs(aResponse) {
        if (aResponse.error) {
          console.error("listTabs failed: " + aResponse.error + " " +
                        aResponse.message);
          aCallback(aState, aResponse);
          return;
        }
        let tab = aResponse.tabs[aResponse.selected];
        aState.dbgClient.attachTab(tab.actor, function (response, tabClient) {
          if (aAttachToWorker) {
            let workerName = "console-test-worker.js#" + new Date().getTime();
            var worker = new Worker(workerName);
            // Keep a strong reference to the Worker to avoid it being
            // GCd during the test (bug 1237492).
            aState._worker_ref = worker;
            worker.addEventListener("message", function listener() {
              worker.removeEventListener("message", listener);
              tabClient.listWorkers(function (response) {
                let worker = response.workers.filter(w => w.url == workerName)[0];
                if (!worker) {
                  console.error("listWorkers failed. Unable to find the " +
                                "worker actor\n");
                  return;
                }
                tabClient.attachWorker(worker.actor, function (response, workerClient) {
                  if (!workerClient || response.error) {
                    console.error("attachWorker failed. No worker client or " +
                                  " error: " + response.error);
                    return;
                  }
                  workerClient.attachThread({}, function (aResponse) {
                    aState.actor = workerClient.consoleActor;
                    aState.dbgClient.attachConsole(workerClient.consoleActor, aListeners,
                                                   _onAttachConsole.bind(null, aState));
                  });
                });
              });
            });
          } else {
            aState.actor = tab.consoleActor;
            aState.dbgClient.attachConsole(tab.consoleActor, aListeners,
                                           _onAttachConsole.bind(null, aState));
          }
        });
      });
    } else {
      aState.dbgClient.getProcess().then(response => {
        aState.dbgClient.attachTab(response.form.actor, function () {
          let consoleActor = response.form.consoleActor;
          aState.actor = consoleActor;
          aState.dbgClient.attachConsole(consoleActor, aListeners,
                                         _onAttachConsole.bind(null, aState));
        });
      });
    }
  });
}

function closeDebugger(aState, aCallback)
{
  aState.dbgClient.close().then(aCallback);
  aState.dbgClient = null;
  aState.client = null;
}

function checkConsoleAPICalls(consoleCalls, expectedConsoleCalls)
{
  is(consoleCalls.length, expectedConsoleCalls.length,
    "received correct number of console calls");
  expectedConsoleCalls.forEach(function (aMessage, aIndex) {
    info("checking received console call #" + aIndex);
    checkConsoleAPICall(consoleCalls[aIndex], expectedConsoleCalls[aIndex]);
  });
}

function checkConsoleAPICall(aCall, aExpected)
{
  if (aExpected.level != "trace" && aExpected.arguments) {
    is(aCall.arguments.length, aExpected.arguments.length,
       "number of arguments");
  }

  checkObject(aCall, aExpected);
}

function checkObject(aObject, aExpected)
{
  for (let name of Object.keys(aExpected))
  {
    let expected = aExpected[name];
    let value = aObject[name];
    checkValue(name, value, expected);
  }
}

function checkValue(aName, aValue, aExpected)
{
  if (aExpected === null) {
    ok(!aValue, "'" + aName + "' is null");
  }
  else if (aValue === undefined) {
    ok(false, "'" + aName + "' is undefined");
  }
  else if (aValue === null) {
    ok(false, "'" + aName + "' is null");
  }
  else if (typeof aExpected == "string" || typeof aExpected == "number" ||
           typeof aExpected == "boolean") {
    is(aValue, aExpected, "property '" + aName + "'");
  }
  else if (aExpected instanceof RegExp) {
    ok(aExpected.test(aValue), aName + ": " + aExpected + " matched " + aValue);
  }
  else if (Array.isArray(aExpected)) {
    info("checking array for property '" + aName + "'");
    checkObject(aValue, aExpected);
  }
  else if (typeof aExpected == "object") {
    info("checking object for property '" + aName + "'");
    checkObject(aValue, aExpected);
  }
}

function checkHeadersOrCookies(aArray, aExpected)
{
  let foundHeaders = {};

  for (let elem of aArray) {
    if (!(elem.name in aExpected)) {
      continue;
    }
    foundHeaders[elem.name] = true;
    info("checking value of header " + elem.name);
    checkValue(elem.name, elem.value, aExpected[elem.name]);
  }

  for (let header in aExpected) {
    if (!(header in foundHeaders)) {
      ok(false, header + " was not found");
    }
  }
}

function checkRawHeaders(aText, aExpected)
{
  let headers = aText.split(/\r\n|\n|\r/);
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

  checkHeadersOrCookies(arr, aExpected);
}

var gTestState = {};

function runTests(aTests, aEndCallback)
{
  function* driver()
  {
    let lastResult, sendToNext;
    for (let i = 0; i < aTests.length; i++) {
      gTestState.index = i;
      let fn = aTests[i];
      info("will run test #" + i + ": " + fn.name);
      lastResult = fn(sendToNext, lastResult);
      sendToNext = yield lastResult;
    }
    yield aEndCallback(sendToNext, lastResult);
  }
  gTestState.driver = driver();
  return gTestState.driver.next();
}

function nextTest(aMessage)
{
  return gTestState.driver.next(aMessage);
}

function withFrame(url) {
  return new Promise(resolve => {
    let iframe = document.createElement("iframe");
    iframe.onload = function () {
      resolve(iframe);
    };
    iframe.src = url;
    document.body.appendChild(iframe);
  });
}

function navigateFrame(iframe, url) {
  return new Promise(resolve => {
    iframe.onload = function () {
      resolve(iframe);
    };
    iframe.src = url;
  });
}

function forceReloadFrame(iframe) {
  return new Promise(resolve => {
    iframe.onload = function () {
      resolve(iframe);
    };
    iframe.contentWindow.location.reload(true);
  });
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

function messageServiceWorker(win, scope, message) {
  return win.navigator.serviceWorker.getRegistration(scope).then(swr => {
    return new Promise(resolve => {
      win.navigator.serviceWorker.onmessage = evt => {
        resolve();
      };
      let sw = swr.active || swr.waiting || swr.installing;
      sw.postMessage({ type: "PING", message: message });
    });
  });
}

function unregisterServiceWorker(win) {
  return win.navigator.serviceWorker.ready.then(swr => {
    return swr.unregister();
  });
}
