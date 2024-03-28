/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global global */

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
this.rpc = function (method, ...params) {
  return new Promise((resolve, reject) => {
    const id = nextId++;
    this.addEventListener("message", function onMessageForRpc(event) {
      const packet = JSON.parse(event.data);
      if (packet.type !== "rpc" || packet.id !== id) {
        return;
      }
      if (packet.error) {
        reject(packet.error);
      } else {
        resolve(packet.result);
      }
      this.removeEventListener("message", onMessageForRpc);
    });

    postMessage(
      JSON.stringify({
        type: "rpc",
        method,
        params,
        id,
      })
    );
  });
}.bind(this);

const { worker } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/worker-loader.sys.mjs",
  { global: "current" }
);

const { WorkerTargetActor } = worker.require(
  "resource://devtools/server/actors/targets/worker.js"
);
const { DevToolsServer } = worker.require(
  "resource://devtools/server/devtools-server.js"
);

DevToolsServer.createRootActor = function () {
  throw new Error("Should never get here!");
};

// This file is only instanciated once for a given WorkerDebugger, which means that
// multiple toolbox could end up using the same instance of this script. In order to handle
// that, we handle a Map of the different connections, keyed by forwarding prefix.
const connections = new Map();

this.addEventListener("message", async function (event) {
  const packet = JSON.parse(event.data);
  switch (packet.type) {
    case "connect":
      const { forwardingPrefix } = packet;

      // Force initializing the server each time on connect
      // as it may have been destroyed by a previous, now closed toolbox.
      // Once the last connection drops, the server auto destroy itself.
      DevToolsServer.init();

      // Step 3: Create a connection to the parent.
      const connection = DevToolsServer.connectToParent(forwardingPrefix, this);

      // Step 4: Create a WorkerTarget actor.
      const workerTargetActor = new WorkerTargetActor(
        connection,
        global,
        packet.workerDebuggerData,
        packet.options.sessionContext
      );
      // Make the worker manage itself so it is put in a Pool and assigned an actorID.
      workerTargetActor.manage(workerTargetActor);

      // Step 5: Send a response packet to the parent to notify
      // it that a connection has been established.
      connections.set(forwardingPrefix, {
        connection,
        workerTargetActor,
      });

      // Immediately notify about the target actor form,
      // so that we can emit RDP events from the target actor
      // and have them correctly routed up to the frontend.
      // The target front has to be first created by receiving its form
      // before being able to receive RDP events.
      postMessage(
        JSON.stringify({
          type: "connected",
          forwardingPrefix,
          workerTargetForm: workerTargetActor.form(),
        })
      );

      // We might receive data to watch.
      if (packet.options.sessionData) {
        const promises = [];
        for (const [type, entries] of Object.entries(
          packet.options.sessionData
        )) {
          promises.push(
            workerTargetActor.addOrSetSessionDataEntry(
              type,
              entries,
              false,
              "set"
            )
          );
        }
        await Promise.all(promises);
      }

      // Finally, notify when we are done processing session data
      // We are processing breakpoints, which means we can release the execution of the worker
      // from the main thread via `WorkerDebugger.setDebuggerReady(true)`
      postMessage(JSON.stringify({ type: "session-data-processed" }));

      break;

    case "add-or-set-session-data-entry":
      await connections
        .get(packet.forwardingPrefix)
        .workerTargetActor.addOrSetSessionDataEntry(
          packet.dataEntryType,
          packet.entries,
          packet.updateType
        );
      postMessage(JSON.stringify({ type: "session-data-entry-added-or-set" }));
      break;

    case "remove-session-data-entry":
      await connections
        .get(packet.forwardingPrefix)
        .workerTargetActor.removeSessionDataEntry(
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
  }
});
