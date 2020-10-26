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
let nextId = 0;
const rpcDeferreds = {};
this.rpc = function(method, ...params) {
  const id = nextId++;

  postMessage(
    JSON.stringify({
      type: "rpc",
      method,
      params,
      id,
    })
  );

  const deferred = defer();
  rpcDeferreds[id] = deferred;
  return deferred.promise;
};

loadSubScript("resource://devtools/shared/worker/loader.js");

const defer = worker.require("devtools/shared/defer");
const { WorkerTargetActor } = worker.require(
  "devtools/server/actors/targets/worker"
);
const { DevToolsServer } = worker.require("devtools/server/devtools-server");

DevToolsServer.init();
DevToolsServer.createRootActor = function() {
  throw new Error("Should never get here!");
};

// This file is only instanciated once for a given WorkerDebugger, which means that
// multiple toolbox could end up using the same instance of this script. In order to handle
// that, we handle a Map of the different connections, keyed by forwarding prefix.
const connections = new Map();

this.addEventListener("message", async function(event) {
  const packet = JSON.parse(event.data);
  switch (packet.type) {
    case "connect":
      const { forwardingPrefix } = packet;

      // Step 3: Create a connection to the parent.
      const connection = DevToolsServer.connectToParent(forwardingPrefix, this);

      // Step 4: Create a WorkerTarget actor.
      const workerTargetActor = new WorkerTargetActor(
        connection,
        global,
        packet.workerDebuggerData
      );
      // Make the worker manage itself so it is put in a Pool and assigned an actorID.
      workerTargetActor.manage(workerTargetActor);

      workerTargetActor.on(
        "worker-thread-attached",
        function onThreadAttached() {
          postMessage(JSON.stringify({ type: "worker-thread-attached" }));
        }
      );
      workerTargetActor.attach();

      // Step 5: Send a response packet to the parent to notify
      // it that a connection has been established.
      connections.set(forwardingPrefix, {
        connection,
        workerTargetActor,
      });

      postMessage(
        JSON.stringify({
          type: "connected",
          forwardingPrefix,
          workerTargetForm: workerTargetActor.form(),
        })
      );

      // We might receive data to watch.
      if (packet.options?.watchedData) {
        const promises = [];
        for (const [type, entries] of Object.entries(
          packet.options.watchedData
        )) {
          promises.push(workerTargetActor.addWatcherDataEntry(type, entries));
        }
        await Promise.all(promises);
      }

      break;

    case "add-watcher-data-entry":
      await connections
        .get(packet.forwardingPrefix)
        .workerTargetActor.addWatcherDataEntry(
          packet.dataEntryType,
          packet.entries
        );
      postMessage(JSON.stringify({ type: "watcher-data-entry-added" }));
      break;

    case "remove-watcher-data-entry":
      await connections
        .get(packet.forwardingPrefix)
        .workerTargetActor.removeWatcherDataEntry(
          packet.dataEntryType,
          packet.entries
        );
      break;

    case "disconnect":
      // This will destroy the associate WorkerTargetActor (and the actors it manages).
      if (connections.has(packet.forwardingPrefix)) {
        connections.get(packet.forwardingPrefix).connection.close();
        connections.delete(packet.forwardingPrefix);
      }
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
