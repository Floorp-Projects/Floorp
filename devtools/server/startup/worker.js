/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/chrome-worker */
/* global worker, loadSubScript, global */

/*
 * Worker debugger script that listens for requests to start a `DevToolsServer` for a
 * worker in a process.  Loaded into a specific worker during worker-connector.js'
 * `connectToWorker` which is called from the same process as the worker.
 */

// This function is used to do remote procedure calls from the worker to the
// main thread. It is exposed as a built-in global to every module by the
// worker loader. To make sure the worker loader can access it, it needs to be
// defined before loading the worker loader script below.
this.rpc = function(method, ...params) {
  const id = nextId++;

  postMessage(
    JSON.stringify({
      type: "rpc",
      method: method,
      params: params,
      id: id,
    })
  );

  const deferred = defer();
  rpcDeferreds[id] = deferred;
  return deferred.promise;
};

loadSubScript("resource://devtools/shared/worker/loader.js");

var defer = worker.require("devtools/shared/defer");
const { Actor } = worker.require("devtools/shared/protocol/Actor");
var { ThreadActor } = worker.require("devtools/server/actors/thread");
var { WebConsoleActor } = worker.require("devtools/server/actors/webconsole");
var { TabSources } = worker.require("devtools/server/actors/utils/TabSources");
var makeDebugger = worker.require("devtools/server/actors/utils/make-debugger");
var { DevToolsServer } = worker.require("devtools/server/devtools-server");
var Targets = worker.require("devtools/server/actors/targets/index");

DevToolsServer.init();
DevToolsServer.createRootActor = function() {
  throw new Error("Should never get here!");
};

var connections = Object.create(null);
var nextId = 0;
var rpcDeferreds = [];

this.addEventListener("message", function(event) {
  const packet = JSON.parse(event.data);
  switch (packet.type) {
    case "connect":
      // Step 3: Create a connection to the parent.
      const connection = DevToolsServer.connectToParent(packet.id, this);

      // Step 4: Create a thread actor for the connection to the parent.
      let sources = null;

      const makeWorkerDebugger = makeDebugger.bind(null, {
        findDebuggees: () => {
          return [this.global];
        },

        shouldAddNewGlobalAsDebuggee: () => {
          return true;
        },
      });

      const targetActorMock = new Actor();
      targetActorMock.initialize(connection);

      const threadActor = new ThreadActor(targetActorMock, global);
      targetActorMock.manage(threadActor);

      Object.assign(targetActorMock, {
        actorID: packet.id,
        targetType: Targets.TYPES.FRAME,

        // threadActor is needed from the webconsole for grip previewing
        threadActor,
        workerGlobal: global,

        onThreadAttached() {
          postMessage(JSON.stringify({ type: "attached" }));
        },

        get dbg() {
          if (!this._dbg) {
            this._dbg = makeWorkerDebugger();
          }
          return this._dbg;
        },
        makeDebugger: makeWorkerDebugger,

        get sources() {
          if (sources === null) {
            sources = new TabSources(threadActor);
          }
          return sources;
        },
      });

      const consoleActor = new WebConsoleActor(connection, targetActorMock);
      targetActorMock.manage(consoleActor);

      // needed so the thread actor can communicate with the console
      // when evaluating logpoints.
      targetActorMock._consoleActor = consoleActor;

      // Step 5: Send a response packet to the parent to notify
      // it that a connection has been established.
      connections[packet.id] = {
        connection,
        rpcs: [],
      };

      postMessage(
        JSON.stringify({
          type: "connected",
          id: packet.id,
          threadActor: threadActor.actorID,
          consoleActor: consoleActor.actorID,
        })
      );
      break;

    case "disconnect":
      connections[packet.id].connection.close();
      break;

    case "rpc":
      const deferred = rpcDeferreds[packet.id];
      delete rpcDeferreds[packet.id];
      if (packet.error) {
        deferred.reject(packet.error);
      }
      deferred.resolve(packet.result);
      break;
  }
});
