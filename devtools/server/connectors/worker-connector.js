/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var DevToolsUtils = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(
  this,
  "MainThreadWorkerDebuggerTransport",
  "devtools/shared/transport/worker-transport",
  true
);

/**
 * Start a DevTools server in a worker and add it as a child server for a given active connection.
 *
 * @params {DevToolsConnection} connection
 * @params {WorkerDebugger} dbg: The WorkerDebugger we want to create a target actor for.
 * @params {String} forwardingPrefix: The prefix that will be used to forward messages
 *                  to the DevToolsServer on the worker thread.
 * @params {Object} options: An option object that will be passed with the "connect" packet.
 * @params {Object} options.sessionData: The sessionData object that will be passed to the
 *                  worker target actor.
 */
function connectToWorker(connection, dbg, forwardingPrefix, options) {
  return new Promise((resolve, reject) => {
    if (!DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
      reject("closed");
      return;
    }

    // Step 1: Ensure the worker debugger is initialized.
    if (!dbg.isInitialized) {
      dbg.initialize("resource://devtools/server/startup/worker.js");

      // Create a listener for rpc requests from the worker debugger. Only do
      // this once, when the worker debugger is first initialized, rather than
      // for each connection.
      const listener = {
        onClose: () => {
          dbg.removeListener(listener);
        },

        onMessage: message => {
          message = JSON.parse(message);
          if (message.type !== "rpc") {
            if (message.type == "worker-thread-attached") {
              // The thread actor has finished attaching and can hit installed
              // breakpoints. Allow content to begin executing in the worker.
              dbg.setDebuggerReady(true);
            }
            return;
          }

          Promise.resolve()
            .then(() => {
              const method = {
                fetch: DevToolsUtils.fetch,
              }[message.method];
              if (!method) {
                throw Error("Unknown method: " + message.method);
              }

              return method.apply(undefined, message.params);
            })
            .then(
              value => {
                dbg.postMessage(
                  JSON.stringify({
                    type: "rpc",
                    result: value,
                    error: null,
                    id: message.id,
                  })
                );
              },
              reason => {
                dbg.postMessage(
                  JSON.stringify({
                    type: "rpc",
                    result: null,
                    error: reason,
                    id: message.id,
                  })
                );
              }
            );
        },
      };

      dbg.addListener(listener);
    }

    if (!DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
      reject("closed");
      return;
    }

    // WorkerDebugger.url isn't always an absolute URL.
    // Use the related document URL in order to make it absolute.
    const absoluteURL = dbg.window?.location?.href
      ? new URL(dbg.url, dbg.window.location.href).href
      : dbg.url;

    // Step 2: Send a connect request to the worker debugger.
    dbg.postMessage(
      JSON.stringify({
        type: "connect",
        forwardingPrefix,
        options,
        workerDebuggerData: {
          id: dbg.id,
          type: dbg.type,
          url: absoluteURL,
        },
      })
    );

    // Steps 3-5 are performed on the worker thread (see worker.js).

    // Step 6: Wait for a connection response from the worker debugger.
    const listener = {
      onClose: () => {
        dbg.removeListener(listener);

        reject("closed");
      },

      onMessage: message => {
        message = JSON.parse(message);
        if (
          message.type !== "connected" ||
          message.forwardingPrefix !== forwardingPrefix
        ) {
          return;
        }

        // The initial connection message has been received, don't
        // need to listen any longer
        dbg.removeListener(listener);

        // Step 7: Create a transport for the connection to the worker.
        const transport = new MainThreadWorkerDebuggerTransport(
          dbg,
          forwardingPrefix
        );
        transport.ready();
        transport.hooks = {
          onTransportClosed: () => {
            if (DevToolsUtils.isWorkerDebuggerAlive(dbg)) {
              // If the worker happens to be shutting down while we are trying
              // to close the connection, there is a small interval during
              // which no more runnables can be dispatched to the worker, but
              // the worker debugger has not yet been closed. In that case,
              // the call to postMessage below will fail. The onTransportClosed hook on
              // DebuggerTransport is not supposed to throw exceptions, so we
              // need to make sure to catch these early.
              try {
                dbg.postMessage(
                  JSON.stringify({
                    type: "disconnect",
                    forwardingPrefix,
                  })
                );
              } catch (e) {
                // We can safely ignore these exceptions. The only time the
                // call to postMessage can fail is if the worker is either
                // shutting down, or has finished shutting down. In both
                // cases, there is nothing to clean up, so we don't care
                // whether this message arrives or not.
              }
            }

            connection.cancelForwarding(forwardingPrefix);
          },

          onPacket: packet => {
            // Ensure that any packets received from the server on the worker
            // thread are forwarded to the client on the main thread, as if
            // they had been sent by the server on the main thread.
            connection.send(packet);
          },
        };

        // Ensure that any packets received from the client on the main thread
        // to actors on the worker thread are forwarded to the server on the
        // worker thread.
        connection.setForwarding(forwardingPrefix, transport);

        resolve({
          workerTargetForm: message.workerTargetForm,
          transport: transport,
        });
      },
    };
    dbg.addListener(listener);
  });
}

exports.connectToWorker = connectToWorker;
